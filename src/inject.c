#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "segment.h"
#include "inject.h"

int grab_text (int pid, int size, void* remote, uint8_t* buffer);
int inject_text (int pid, int size, void* remote, const uint8_t* text);

void print_siginfo (siginfo_t* ps)
{
	printf("siginfo.si_signo = %d   siginfo.si_code = %d   siginfo.si_errno = %d\n",
				 ps->si_signo, ps->si_code, ps->si_errno);
}

int inject_and_run_text (int pid, int size, const uint8_t* text)
{
  int r = 0;
	struct user regset;

	/* save state */
	if (save_remote_registers(pid, &regset)) {
		return 1;
	}

	void* rip = (void*)regset.regs.rip;
	uint8_t* savebuf = (uint8_t*)alloca(size);
	memset(savebuf, 0, size);

	if (grab_text(pid, size, rip, savebuf)) {
		return 2;
	}

	/* copy code into remote process at IP and run it */
	if (inject_text(pid, size, rip, text)) {
		return 3;
	}
	
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		return 4;
	}
	
	/* Wait for the program to issue SIGSTOP to itself
	 * (the text stub must end with a signal).
   * If the program doesn't SIGSTOP then we end up waiting forever!
	 */
	siginfo_t siginfo;
	do {
		int result = waitid(P_PID, pid, &siginfo, WSTOPPED);
		if (result == -1) {
			return 5;
		}
		if (siginfo.si_code == CLD_STOPPED) {
			break;
		}
		if (siginfo.si_code == CLD_EXITED ||
				siginfo.si_code == CLD_KILLED ||
				siginfo.si_code == CLD_DUMPED) {
			return 6;
		}
	} while(siginfo.si_code != CLD_STOPPED);

	if (inject_text(pid, size, (void*)regset.regs.rip, savebuf)) {
		return 7;
	}

	if (restore_remote_registers(pid, &regset)) {
		return 8;
	}
		
	return 0;
}

int save_remote_registers (int pid, struct user* ur)
{
	if (ptrace(PTRACE_GETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_GETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	return 0;
}

int restore_remote_registers (int pid, struct user* ur)
{
	if (ptrace(PTRACE_SETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_SETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	
	return 0;
}

int grab_text (int pid, int size, void* remote, uint8_t* buffer)
{
	assert (size % sizeof(long) == 0);

	for (int i=0; i < size; i += sizeof(long)) {
		long peeked = ptrace(PTRACE_PEEKTEXT, pid, (void*)(remote+i), NULL);
		if (peeked == -1) {
			return 1;
		}
		*(long*)(buffer+i) = peeked;
	}
	
	return 0;
}

int inject_text (int pid, int size, void* rip, const uint8_t* text)
{
	// Note that 'text' is a pointer to codetext,
	// but that what POKETEXT wants is an actual _value_ passed as its second parameter, but cast as a void*.
	// So you have to convert the text pointer to a uint64_t pointer and dereference it to get the uint64_t,
	//    then treat that as (void*).
	
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		if (ptrace(PTRACE_POKETEXT, pid, (void*)(rip + i), (void*)*(uint64_t*)(text+i) ) == -1) {
			return 1;
		}
	}
	
	return 0;
}

/**
	 Based on the following disassembled snippet from objdump:
	 be 00 00 00 00       	       mov    $0x2,%esi
	 48 8d 3d e9 06 00 00 	       lea    0x6e9(%rip),%rdi          # 15f8 <_IO_stdin_used+0xa8>
	 48 a3 01 02 03 04 05 06 07 08 movabs %rax, 0x0807060504030201
	 ff d0                         call   *%rax
	 cc                            int    3
	 
	 1. The lea line is changed to "lea 0x13(%rip),%rdi"  in order to grab the injected text string
	 2. The callq line is changed to accept the new address computed from the symbol table of dlopen and the memory map of the tracee
   3. A software breakpoint is inserted at byte 18.

 */
void* inject_dlopen (int pid, const char* shlib_path, uint32_t flags)
{
	const int arg2_offset = 1;
	const int arg1_offset = 32;
	const int funcaddr_offset = 14;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	const int strsize = 512;
	char unitpath[strsize];
	char symbol[strsize];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	unsigned long segaddr, segaddr2;
	unsigned long segoffset;
	unsigned long symofs;
	uint8_t r;
	unsigned long factsegaddr, factsegaddr2;
	unsigned long factsegsize;
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\xbe\x02\x00\x00\x00"                      // mov    $0x2,%esi
				 "\x48\x8d\x3d\x14\x00\x00\x00"              // lea    0x14(%rip),%rdi
				 "\x48\xb8\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 25);
	strncpy((uint8_t*) buffer + arg1_offset, shlib_path, bufsz-arg1_offset);
	*(int32_t*)(buffer+arg2_offset) = flags;
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	find_segment_address_regex(pid, "/libdl", unitpath, strsize, &segaddr, &segaddr2, &segoffset);
  find_symbol_offset_regex(unitpath, "dlopen@@", symbol, strsize, &symofs);
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + segoffset + symofs;
	printf("dlopen is located at 0x%lx\n", segaddr + segoffset + symofs);
	
	// save remote registers and existing rip text
	if (r = save_remote_registers(pid, &ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = grab_text (pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	inject_text (pid, bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		fprintf(stderr,"Couldn't continue the tracee.\n");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	printf("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		printf("Stopped. WSTOPSIG(wstatus) = %d\n", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = save_remote_registers(pid, &ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = inject_text(pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	restore_remote_registers (pid, &ur)) {
		fprintf(stderr, "Problem restoring saved registers: %d\n", r);
		return (void*)6;
	}

	return (void*)ur2.regs.rax;
}


void* inject_dlsym (int pid, const char* symbolname)
{
	const int arg1_offset = 32;
  const int funcaddr_offset = 9;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	const int strsize = 512;
	char unitpath[strsize];
	char symbol[strsize];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	unsigned long segaddr, segaddr2;
	unsigned long segoffset;
	unsigned long symofs;
	uint8_t r;
	unsigned long factsegaddr, factsegaddr2;
	unsigned long factsegsize;

	// find dlsym's address and name
	find_segment_address_regex(pid, "/libdl", unitpath, strsize, &segaddr, &segaddr2, &segoffset);
  find_symbol_offset_regex(unitpath, "dlsym@@", symbol, strsize, &symofs);
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\x48\x8d\x3d\x20\x00\x00\x00"              // lea    0x20(%rip),%rdi
				 "\x48\xb8\x01\x02\x03\x04\x05\x06\x07\x08"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 20);
	strncpy((uint8_t*) buffer + arg1_offset, symbol, bufsz-arg1_offset);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + segoffset + symofs;
	printf("dlsym is located at 0x%lx\n", segaddr + segoffset + symofs);
	
	// save remote registers and existing rip text
	if (r = save_remote_registers(pid, &ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = grab_text (pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	inject_text (pid, bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		fprintf(stderr,"Couldn't continue the tracee.\n");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	printf("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		printf("Stopped. WSTOPSIG(wstatus) = %d\n", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = save_remote_registers(pid, &ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = inject_text(pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	restore_remote_registers (pid, &ur)) {
		fprintf(stderr, "Problem restoring saved registers: %d\n", r);
		return (void*)6;
	}

	return (void*)ur2.regs.rax;
	
}

void* inject_dlerror (int pid)
{
	const int arg1_offset = 32;
  const int funcaddr_offset = 2;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	const int strsize = 512;
	char unitpath[strsize];
	char symbol[strsize];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	unsigned long segaddr, segaddr2;
	unsigned long segoffset;
	unsigned long symofs;
	uint8_t r;
	unsigned long factsegaddr, factsegaddr2;
	unsigned long factsegsize;

	// find dlsym's address and name
	find_segment_address_regex(pid, "/libdl", unitpath, strsize, &segaddr, &segaddr2, &segoffset);
  find_symbol_offset_regex(unitpath, "dlerror@@", symbol, strsize, &symofs);
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\x48\xb8\x01\x02\x03\x04\x05\x06\x07\x08"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 13);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + segoffset + symofs;
	printf("dlerror is located at 0x%lx\n", segaddr + segoffset + symofs);
	
	// save remote registers and existing rip text
	if (r = save_remote_registers(pid, &ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = grab_text (pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	inject_text (pid, bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		fprintf(stderr,"Couldn't continue the tracee.\n");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	printf("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		printf("Stopped. WSTOPSIG(wstatus) = %d\n", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = save_remote_registers(pid, &ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = inject_text(pid, bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	restore_remote_registers (pid, &ur)) {
		fprintf(stderr, "Problem restoring saved registers: %d\n", r);
		return (void*)6;
	}

	printf("Continuing tracee...\n");
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		fprintf(stderr, "Couldn't continue the tracee after running code injection.\n");
		return (void*)7;
	}

	return (void*)ur2.regs.rax;
	
}

