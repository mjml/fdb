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
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

#include "segment.h"
#include "Inject.hpp"

static void print_siginfo (siginfo_t* ps)
{
	printf("siginfo.si_signo = %d   siginfo.si_code = %d   siginfo.si_errno = %d\n",
				 ps->si_signo, ps->si_code, ps->si_errno);
}


SymbolTable::SymbolTable (std::string& _path) 
	: path(_path)
{
	Parse(path);
};

SymbolTable::~SymbolTable () {}


void SymbolTable::Parse (const std::string& _path)
{
	char cmd[1024];

	offsets.clear();
	
	snprintf(cmd, 1024, "readelf -Ws %s", path.c_str());
	FILE* cmdout = popen(cmd, "r");

	int r = 0;
	char* line = nullptr;
	size_t n = 0;
	
	for (int i=0; i < 3; i++) {
		r = getline(&line, &n, cmdout);
		free(line);
		line = nullptr;
	}
	
	do {
		n = 0;
		r = getline(&line, &n, cmdout);
		if (r == 0) {
			free(line);
			line = nullptr;
			continue;
		}
		uint64_t offset = 0;
		char sym_name[1024];
		sym_name[0] = 0;
		int c = sscanf(line, "%*d: %lx %*d %*s %*s %*s %*s %1023s %*s", &offset, sym_name);
		if (c == 2) {
			offsets.emplace(std::string(sym_name), offset);
		}
		free(line);
		line = nullptr;
		
	} while (r >= 0);
	
	pclose(cmdout);
}


uint64_t SymbolTable::FindSymbolOffsetByPattern (const char* regex_pattern)
{
	regex_t regex;
	uint64_t result = 0;
	regcomp(&regex, regex_pattern, REG_EXTENDED);
	for (auto it = offsets.begin(); it != offsets.end(); it++) {
		if (!regexec(&regex, it->first.c_str(), 0, nullptr, 0)) {
			result = it->second;
		}
	}
	regfree(&regex);
	return result;
}


Tracee* Tracee::FindByNamePattern (const char* regex_pattern)
{
	int result=0;
	regex_t regex;
	regcomp (&regex, regex_pattern, REG_EXTENDED);
	
	DIR* procdir = opendir("/proc");
	struct dirent* de = readdir(procdir);
	while (de) {
		int pid = atoi(de->d_name);
		if (!pid) {
			de = readdir(procdir);
			continue;
		}

		char cmdpathfn[1024];
		snprintf(cmdpathfn, 1024, "/proc/%d/comm", pid);

		std::fstream fs(cmdpathfn, std::ios::in);
		std::string str;
		fs >> str;

		if (regexec(&regex, str.c_str(), 0, nullptr, 0) == 0) {
			result = pid;
			fs.close();
			break;
		}
		
		fs.close();
		de = readdir(procdir);
	}
	
	regfree(&regex);

	if (result == 0) {
		throw std::runtime_error("Could not find process");
	}
	
	return new Tracee(result);
}

void Tracee::Attach ()
{
	if (!pid) {
		char msg[1024];
		snprintf(msg,1024, "Error: no pid defined to attach to\n", strerror(errno));
		throw std::runtime_error(msg);
	}
	
	if (ptrace(PTRACE_SEIZE,pid,0,PTRACE_O_TRACESYSGOOD) == -1) {
		char msg[1024];
		snprintf(msg,1024, "Error attaching to process: %s\n", strerror(errno));
		throw std::runtime_error(msg);
	}
}

int Tracee::SaveRemoteRegisters (struct user* ur)
{
	if (ptrace(PTRACE_GETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_GETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	return 0;

}


int Tracee::RestoreRemoteRegisters (struct user* ur)
{
	if (ptrace(PTRACE_SETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_SETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	
	return 0;
}


int Tracee::rbreak ()
{

	// Send interrupt
	printf("Sending interrupt to %d.\n", pid);
	if (ptrace(PTRACE_INTERRUPT,pid,0,0) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(2);
	}
	
	int wstatus;
	siginfo_t siginfo;
	struct user ur;

	int syscallstops = 0;
	
	do {
		
		waitpid(pid, &wstatus,0);
		printf("Stopped: ");

		//SaveRemoteRegisters(&ur);
		
		ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo);
		if (WIFSTOPPED(wstatus)) {
			printf("# STOP received. rip=0x%lx  rbp=0x%lx  rsp=0x%lx\n", ur.regs.rip, ur.regs.rbp, ur.regs.rsp);
			printf("siginfo.si_info is 0x%lx\n", siginfo.si_code);
			if (WSTOPSIG(wstatus) == (SIGTRAP|0x80)) {
				// this indicates that we're inside a syscall-enter-stop
				syscallstops++;
				printf("wstatus == SIGTRAP|0x80\n");
			} else if (WSTOPSIG(wstatus) == SIGTRAP) {
				//syscallstops++;
				printf("wstatus == SIGTRAP\n");
				// this indicates that we're inside a syscall-exit stop
			} else {
				printf("WSTOPSIG(wstatus) == 0x%lx\n", WSTOPSIG(wstatus));
			}
			if (syscallstops < 2 || ur.regs.rbp < 0x1000) {
				ptrace(PTRACE_SYSCALL,pid,0,0);
				continue;
			} else {
				break;
			}
		} else {
			// exit or termination
			fprintf(stderr, "Signal received by debugger but tracee is not stopped. Exiting.\n");
			return 1;
		}

		asm("nop;");

	} while (1);

	return 0;

}

int Tracee::rcont ()
{
	return ptrace(PTRACE_CONT, pid, 0, 0);
}


int Tracee::GrabText (int size, void* rip, uint8_t* buffer)
{
	assert (size % sizeof(long) == 0);

	for (int i=0; i < size; i += sizeof(long)) {
		long peeked = ptrace(PTRACE_PEEKTEXT, pid, (uint64_t)(rip)+i, NULL);
		if (peeked == -1) {
			return 1;
		}
		*(long*)(buffer+i) = peeked;
	}
	
	return 0;
	
}


std::string Tracee::FindRemoteExecutablePath ()
{
	assert (pid);
	char exelink[1024];
	snprintf(exelink, 1024, "/proc/%d/exe", pid);
	char exepath[1024];
	memset(exepath, 0, 1024);
	readlink(exelink, exepath, 1024);
	return std::string(exepath);
}


uint64_t Tracee::FindRemoteSymbolOffsetByPattern (const char* regex_pattern)
{
	char cmd[1024];
	uint64_t result = 0L;

	std::string path = FindRemoteExecutablePath();
	SymbolTable stable(path);

	result = stable.FindSymbolOffsetByPattern(regex_pattern);

	return result;
}


int Tracee::InjectText (int size, void* rip, const uint8_t* text)
{
	// Note that 'text' is a pointer to codetext,
	// but that what POKETEXT wants is an actual _value_ passed as its second parameter, but cast as a void*.
	// So you have to convert the text pointer to a uint64_t pointer and dereference it to get the uint64_t,
	//    then treat that as (void*).
	
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		if (ptrace(PTRACE_POKETEXT, pid, (uint64_t)rip+i, (void*)*(uint64_t*)(text+i) ) == -1) {
			return 1;
		}
	}
	
	return 0;
}



int Tracee::InjectAndRunText (int size, const uint8_t* text)
{
  int r = 0;
	struct user regset;

	/* save state */
	if (SaveRemoteRegisters(&regset)) {
		return 1;
	}

	void* rip = (void*)regset.regs.rip;
	uint8_t* savebuf = (uint8_t*)alloca(size);
	memset(savebuf, 0, size);

	if (GrabText(size, rip, savebuf)) {
		return 2;
	}

	/* copy code into remote process at IP and run it */
	if (InjectText(size, rip, text)) {
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

	if (InjectText(size, (void*)regset.regs.rip, savebuf)) {
		return 7;
	}

	if (RestoreRemoteRegisters(&regset)) {
		return 8;
	}
		
	return 0;

}


void* Tracee::Inject_dlopen (const char* shlib_path, uint32_t flags)
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
	strncpy((char*)buffer + arg1_offset, shlib_path, bufsz-arg1_offset);
	*(int32_t*)(buffer+arg2_offset) = flags;
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	find_segment_address_regex(pid, "/libdl", unitpath, strsize, &segaddr, &segaddr2, &segoffset);
  find_symbol_offset_regex(unitpath, "dlopen@@", symbol, strsize, &symofs);
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + segoffset + symofs;
	printf("dlopen is located at 0x%lx\n", segaddr + segoffset + symofs);
	
	// save remote registers and existing rip text
	if (r = SaveRemoteRegisters(&ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	InjectText (bufsz, rip, buffer);
	
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
	if (r = SaveRemoteRegisters(&ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = InjectText(bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	RestoreRemoteRegisters (&ur)) {
		fprintf(stderr, "Problem restoring saved registers: %d\n", r);
		return (void*)6;
	}
	
	return (void*)ur2.regs.rax;

}

void* Tracee::Inject_dlsym (const char* szsymbol)
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
	strncpy((char*)((uint8_t*)buffer + arg1_offset), symbol, bufsz-arg1_offset);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	*((uint64_t*)(buffer+funcaddr_offset)) = segaddr + segoffset + symofs;
	printf("dlsym is located at 0x%lx\n", segaddr + segoffset + symofs);
	
	// save remote registers and existing rip text
	if (r = SaveRemoteRegisters(&ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	InjectText (bufsz, rip, buffer);
	
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
	if (r = SaveRemoteRegisters(&ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = InjectText(bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	RestoreRemoteRegisters (&ur)) {
		fprintf(stderr, "Problem restoring saved registers: %d\n", r);
		return (void*)6;
	}

	return (void*)ur2.regs.rax;
}

void* Tracee::Inject_dlerror ()
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
	if (r = SaveRemoteRegisters(&ur)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		fprintf(stderr, "Problem grabbing text: %d\n", r);
		return (void*)2;
	}

	// inject the blob
	printf("Injecting codetext...\n");
	InjectText (bufsz, rip, buffer);
	
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
	if (r = SaveRemoteRegisters(&ur2)) {
		fprintf(stderr, "Problem saving remote registers: %d\n", r);
		return (void*)4;
	}

	printf("rax value: 0x%lx\n", ur2.regs.rax);
	printf("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	printf("Restoring previous codetext\n");
	if (r = InjectText(bufsz, rip, saved)) {
		fprintf(stderr, "Problem injecting saved codetext: %d\n", r);
		return (void*)5;
	}

	printf("Restoring previous registers\n");
	if (r =	RestoreRemoteRegisters (&ur)) {
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

void Tracee::Kill (int signal)
{
	kill(pid, signal);
}
