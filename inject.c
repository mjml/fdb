#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <alloca.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <regex.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "inject.h"

int grab_text (int pid, int size, void* remote, uint8_t* buffer);
int inject_text (int pid, int size, void* remote, const uint8_t* text);

static long find_segment_address_regex (int pid, const char* unitpath_regex, char* oname, int noname,
																			 unsigned long* paddr, unsigned long* paddr2, unsigned long* poffset);
static long find_symbol_offset_regex (const char* unitpath, const char* symbol_regex, char* sname, int nsname,
																		 unsigned long* poffset);

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
	const int funcaddr_offset = 15;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	const int strsize = 512;
	char unitpath[strsize];
	char symbol[strsize];
	struct user ur;
	siginfo_t siginfo;
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
				 "\x48\x8d\x3d\x1b\x00\x00\x00"              // lea    0x1b(%rip),%rdi
				 "\x48\xb8\x01\x02\x03\x04\x05\x06\x07\x08"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 25);
	strncpy((uint8_t*) buffer + arg1_offset, shlib_path, bufsz-arg1_offset);
	*(int32_t*)(buffer+arg2_offset) = flags;
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	find_segment_address_regex(pid, "/libdl", unitpath, strsize, &segaddr, &segaddr2, &segoffset);
  find_symbol_offset_regex(unitpath, "dlopen@@", symbol, strsize, &symofs);
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + symofs;
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
	printf("Waiting for completion...\n");
	waitid(P_PID, pid, &siginfo, WSTOPPED);
	
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


void* inject_dlsym (int pid, const char* szsym)
{
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	struct user ur;
	int r;
	void *rip;
	
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





/**
 * Returns the execution unit's address, or 0 for not found or UINT64_MAX for error.
 */
long find_segment_address_regex (int pid, const char* unitpath_regex, char* oname, int noname,
																 unsigned long* paddr, unsigned long* paddr2, unsigned long* poffset)
{
	const int strsize = 512;
	char path[strsize];

	regex_t re;
	if (regcomp(&re, unitpath_regex, REG_EXTENDED)) {
		return 1;
	}

	snprintf(path, strsize, "/proc/%d/maps", pid);
	FILE* fmap = fopen(path, "r");
	if (fmap == NULL) {
		return 2;
	}

	char* r;
	
	do {
		unsigned long saddr, saddr2;
		unsigned long offs;
		char privs[6];
		char objname[strsize];
		char line[strsize];
		int n=0;
		objname[0] = 0;
		r = fgets(line, strsize, fmap);
		n = sscanf(line, "%lx - %lx %4s %lx %*x:%*x %*d %s\n", &saddr, &saddr2, privs, &offs, objname);
		if (n == 2) continue;
		if (!regexec(&re,objname,(size_t)0,NULL,0) && strchr(privs, 'x')) {
			*paddr = saddr;
			if (paddr2) *paddr2 = saddr2;
			if (poffset) *poffset = offs;
			printf("Found segment %s at 0x%lx.\n", objname, *paddr);
			strncpy(oname, objname, noname);
			break;
		}
	} while (r != NULL);

	//if (segment_address == 0) {
	//	printf("No match found.\n");
	//}
	
	fclose(fmap);
	regfree(&re);
	
	return 0;
	
}


/**
 * 
 */
static long find_symbol_offset_regex (const char* unitpath, const char* symbol_regex, char* sname, int nsname,
																			unsigned long* poffset)
{
	const int strsize = 512;
	char readelf_cmd[strsize];
	char line[strsize];
	regex_t re_symbol;
	FILE* fhre;
	int n;
	char* r;
	
	if (regcomp(&re_symbol, symbol_regex, REG_EXTENDED)) {
		return 1;
	}
	
	snprintf(readelf_cmd, strsize, "readelf -s %s", unitpath);
	//printf("# %s\n", readelf_cmd);
	fhre = popen(readelf_cmd, "r");
	do {
		uint64_t sofs;
		char symname[strsize];
		r = fgets(line, strsize, fhre);
		n = sscanf(line, "%*d: %lx %*d %*s %*s %*s %*s %s", &sofs, symname);
		/*
		if (n==2) {
			printf("#%s", line);
		} else {
			printf(line);
		} 
		*/
		if (!regexec(&re_symbol,symname,(size_t)0,NULL,0)) {
			strncpy(sname, symname, nsname);
			*poffset = sofs;
			printf("Found symbol %s offset at 0x%lx.\n", symname, *poffset);
			break;
		}
	} while (r != NULL);
	
	pclose(fhre);
	regfree(&re_symbol);
	return 0;
	
}
