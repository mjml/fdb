#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <alloca.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

int save_remote_registers (int pid, struct user* ur);
int restore_remote_registers (int pid, struct user* ur);
int grab_text (int pid, int size, void* remote, uint8_t* buffer);
int inject_text (int pid, int size, void* remote, const uint8_t* text);

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
	 2. The callq line is changed to accept the new address from the plt (or .got) of the 
   3. A software breakpoint is inserted at byte 18.

 */
int make_dlopen (const char* shlib_path, uint8_t flags, uint8_t* buffer, int bufsiz)
{
	const int str_offset = 24;
	assert(bufsiz >= 256);
	assert(strlen(shlib_path) < 256-str_offset-1);

	memset(buffer, 0, bufsiz);
	memcpy(buffer, (uint8_t*) "\xbe\x02\x00\x00\x00\x48\x8d\x3d\x0d\x00\x00\x00\xe8\xbc\xfc\xff\xff", 18);
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
		
	make_dlopen(shlib_path, flags, buffer, bufsz);

	if (ptrace(PTRACE_INTERRUPT, pid, 0, 0) == -1) {
		return 1;
	}
	
	save_remote_registers(pid, &ur);
	void* rip = (void*)ur.regs.rip;

	grab_text (pid, bufsz, rip, saved);
	inject_text (pid, bufsz, rip, buffer);

	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		return 2;
	}

	waitid(P_PID, pid, &siginfo, WSTOPPED);
	
	inject_text(pid, bufsz, rip, saved);
	restore_remote_registers (pid, &ur);

	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		return 3;
	}
	
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
