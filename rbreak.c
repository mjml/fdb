#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include "inject.h"

int rbreak (int pid)
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

		save_remote_registers(pid, &ur);
		
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

int multibreak (int pid, int n)
{

	// Send interrupt
	printf("Sending PTRACE_INTERRUPT to %d.\n", pid);
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

		save_remote_registers(pid, &ur);
		
		ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo);
		if (WIFSTOPPED(wstatus)) {
			printf("# STOP received. rip=0x%lx  rbp=0x%lx\n", ur.regs.rip, ur.regs.rbp);
			printf("siginfo.si_code = 0x%lx\n", siginfo.si_code);
			if (WSTOPSIG(wstatus) == SIGTRAP) {
				printf("WSTOPSIG(wstatus) == SIGTRAP\n");
			} else if (WSTOPSIG(wstatus) == (SIGTRAP|0x80)) {
				
				// this indicates that we're inside a syscall-exit-stop
				syscallstops++;
				printf("WSTOPSIG(wstatus) == SIGTRAP|0x80\n");
				
			} else {
				
				printf("WSTOPSIG(wstatus) == 0x%lx\n", WSTOPSIG(wstatus));
				
			}

			if (syscallstops < 2) {
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


int rcont (int pid)
{
	return ptrace(PTRACE_CONT, pid, 0, 0);
}
