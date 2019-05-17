#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include "rbreak.h"
#include "inject.h"

int target_pid;

void cleanup ();

int main (int argc, char* argv[])
{
	printf("---------------\n");
	printf("factinject v0.2\n");
	printf("---------------\n");
	fflush(stdout);

#ifndef DEBUG_CHILD
  // Find the attachee process
	target_pid = find_pid_by_pattern ("^factorio$");
	if (!target_pid) {
		fprintf(stderr, "Coudln't attach to target process. Exiting.\n");
		fflush(stderr);
		return 1;
	}
#else
	target_pid = fork();
	if (target_pid == 0) {
		execl("./exp2", "exp2", NULL);
		return 0;
	} else {
		printf("Forked pid %d\n", target_pid);
		sleep(1);
	}
#endif

	// Attach via ptrace
	printf("Attaching to process %d. ", target_pid);
	if (ptrace(PTRACE_SEIZE,target_pid,0,PTRACE_O_TRACESYSGOOD) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(1);
	}

	printf("Success.\n");
	fflush(stdout);

	// Traps the tracee
	rbreak(target_pid);
  		
	// Inject self
	printf("Inject current executable into tracee.\n");
  inject_dlopen(target_pid, "/home/joya/localdev/factinject/factinject", RTLD_NOW | RTLD_GLOBAL);
	//inject_dlopen(target_pid, "libunwind.so", RTLD_NOW);
	printf("Done.\n");

	printf("Continuing...\n");
	rcont(target_pid);
	sleep(1);

	
	rbreak(target_pid);
	printf("Calling dlerror\n");
	inject_dlerror(target_pid);
	printf("Done.\n");
	
		
	// Continue tracee
	if (ptrace(PTRACE_CONT,target_pid,0,0) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(3);
	}

	while (1) {
		
	}
	
	// All done.
	cleanup();
	
	return 0;
}


void test_func ()
{
	printf("Hello, PIE world!\n");
}


void cleanup()
{
	kill(target_pid, SIGKILL);
}

char factinjectpath[] = "/home/joya/localdev/factinject/factinject";

void bootstrap_inject ()
{
	dlopen(factinjectpath, RTLD_NOW);
}
