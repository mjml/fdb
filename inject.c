#include <stdio.h>
#include <stdlib.h>
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

int save_remote_registers (int pid, struct user* ur);
int restore_remote_registers (int pid, struct user* ur);
int grab_text (int pid, int size, void* remote, uint8_t* buffer);
int inject_text (int pid, int size, void* remote, const uint8_t* text);

static uint64_t find_segment_address_regex (int pid, const char* unitpath_regex, char* oname, int noname);
static uint64_t find_symbol_address_regex (const char* unitpath, const char* symbol_regex, char* sname, int nsname);

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
		if (ptrace(PTRACE_PEEKTEXT, pid, (void*)(remote+i), (void*)(buffer+i)) == -1) {
			return 1;
		}
	}
	
	return 0;
}

int inject_text (int pid, int size, void* rip, const uint8_t* text)
{
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		if (ptrace(PTRACE_POKETEXT, pid, (void*)(rip + i), (void*)(text+i)) == -1) {
			return 1;
		}
	}
	
	return 0;
	
}

/**
	 Based on the following disassembled snippet from objdump:

0000000000000eff <bootstrap_inject>:
     eff:	55                   	push   %rbp
     f00:	48 89 e5             	mov    %rsp,%rbp
-- start here --
     f03:	be 02 00 00 00       	mov    $0x2,%esi
     f08:	48 8d 3d e9 06 00 00 	lea    0x6e9(%rip),%rdi        # 15f8 <_IO_stdin_used+0xa8>
     f0f:	e8 bc fc ff ff       	callq  bd0 <dlopen@plt>
-- end here --
     f14:	90                   	nop
     f15:	5d                   	pop    %rbp
     f16:	c3                   	retq   
 
	 1. The lea line is changed to "lea 0x13(%rip),%rdi"  in order to grab the injected text string
	 2. The callq line is changed to accept the new address computed from the symbol table of dlopen and the memory map of the tracee
   3. A software breakpoint is inserted at byte 18.

 */
int make_dlopen (const char* shlib_path, uint8_t flags, uint8_t* buffer, int bufsiz)
{
	const int str_offset = 24;
	assert(bufsiz >= 256);
	assert(strlen(shlib_path) < 256-str_offset-1);

	memset(buffer, 0, bufsiz);
	memcpy(buffer, (uint8_t*) "\xbe\x02\x00\x00\x00\x48\x8d\x3d\x0d\x00\x00\x00\xe8\xbc\xfc\xff\xff\x90\xcc", 19);
	buffer[1] = flags;
	strncpy((uint8_t*) buffer + str_offset, shlib_path, 256-str_offset-1);
	
	return 0;
}

int inject_dlopen (int pid, const char* shlib_path, uint8_t flags)
{
	const int bufsz = 512;
	uint8_t buffer[512];
	uint8_t saved[512];
	struct user ur;
	siginfo_t siginfo;
	
	// create the injected blob
	make_dlopen(shlib_path, flags, buffer, bufsz);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	const int strsize = 512;
	char unitpath[strsize];
	char symbol[strsize];
	uint64_t segaddr = find_segment_address_regex(pid, "/libdl", unitpath, strsize);
	uint64_t symofs = find_symbol_address_regex(unitpath, "dlopen@@", symbol, strsize);
	
	
	// save remote registers and existing rip text
	save_remote_registers(pid, &ur);
	void* rip = (void*)ur.regs.rip;
	grab_text (pid, bufsz, rip, saved);
	
	/*
	// inject the blob
	inject_text (pid, bufsz, rip, buffer);

	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		return 2;
	}

	// wait for the tracee to stop
	waitid(P_PID, pid, &siginfo, WSTOPPED);
	*/

	// restore tracee rip text and registers
	/*
	inject_text(pid, bufsz, rip, saved);
	restore_remote_registers (pid, &ur);
	*/

	return 0;
}


/**
 * For very simple functions (with no parameters or calls), create stub code that contains 
 * the function without the prologue. 
 */
int func_to_stub (void (*func)(), uint8_t* buffer, size_t bufsiz)
{
	uint8_t* funcdata = (uint8_t*) func;

	// check that the function has the standard prologue
	uint32_t prologue = 0xe5894855;
	uint32_t* pi32_funcdata = (uint32_t*)funcdata;
	if (prologue != *pi32_funcdata) {
		// doesn't have usual call semantics
		return 1;
	}
	
	// skip past prologue, copy bytes until return
	funcdata += 4;
	
	uint16_t* pi16_funcdata;
	int i;
	for (i=0; i < bufsiz; i++) {
		pi16_funcdata = (uint16_t*)funcdata;
		if (*pi16_funcdata == 0xc35d) {
			buffer[i] = 0xcc; // write in an INT 3 to break the program after the stub
			break;
		} else {
			buffer[i] = *funcdata; // otherwise write function data
			funcdata++;
		}
	}
	
	// if we didn't have enough space to contain the function in the stub buffer, return an error.
	if (*funcdata != 0xc3 && i == bufsiz) {
		return 2;
	}

	return 0;
}


/**
 * Returns the execution unit's address, or 0 for not found or UINT64_MAX for error.
 */
uint64_t find_segment_address_regex (int pid, const char* unitpath_regex, char* oname, int noname)
{
	const int strsize = 512;
	char path[strsize];
	uint64_t segment_address = 0;

	regex_t re;
	if (regcomp(&re, unitpath_regex, REG_EXTENDED)) {
		return UINT64_MAX;
	}

	snprintf(path, strsize, "/proc/%d/maps", pid);
	FILE* fmap = fopen(path, "r");
	if (fmap == NULL) {
		return UINT64_MAX;
	}

	char* r;
	
	do {
		unsigned long saddr;
		char privs[6];
		char objname[strsize];
		char line[strsize];
		int n=0;
		objname[0] = 0;
		r = fgets(line, strsize, fmap);
		n = sscanf(line, "%lx - %*x %4s %*x %*x:%*x %*d %s\n", &saddr, privs, objname);
		if (n == 2) continue;
		if (!regexec(&re,objname,(size_t)0,NULL,0) && strchr(privs, 'x')) {
			segment_address = saddr;
			printf("Found segment at 0x%lx.\n", segment_address);
			strncpy(oname, objname, noname);
			break;
		}
	} while (r != NULL);

	//if (segment_address == 0) {
	//	printf("No match found.\n");
	//}
	
	fclose(fmap);
	regfree(&re);
	
	return segment_address;
	
}


/**
 * 
 */
static uint64_t find_symbol_address_regex (const char* unitpath, const char* symbol_regex, char* sname, int nsname)
{
	const int strsize = 512;
	char readelf_cmd[strsize];
	char line[strsize];
	regex_t re_symbol;
	FILE* fhre;
	int n;
	char* r;
	uint64_t symbol_offset;
	
	if (regcomp(&re_symbol, symbol_regex, REG_EXTENDED)) {
		return UINT64_MAX;
	}
	
	snprintf(readelf_cmd, strsize, "readelf -s %s", unitpath);
	printf("# %s\n", readelf_cmd);
	fhre = popen(readelf_cmd, "r");
	do {
		uint64_t sofs;
		char symname[strsize];
		r = fgets(line, strsize, fhre);
		n = sscanf(line, "%*d: %lx %*d %*s %*s %*s %*s %s", &sofs, symname);
		if (n==2) {
			printf("#%s", line);
		} else {
			printf(line);
		} 
		if (!regexec(&re_symbol,symname,(size_t)0,NULL,0)) {
			strncpy(sname, symname, nsname);
			symbol_offset = sofs;
			printf("Found symbol offset at 0x%lx.\n", symbol_offset);
			break;
		}
	} while (r != NULL);
	
	pclose(fhre);
	regfree(&re_symbol);
	return symbol_offset;
	
}
