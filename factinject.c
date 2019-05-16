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

#include "findpid.h"
#include "rbreak.h"
#include "inject.h"

int target_pid;

void cleanup ();

int main (int argc, char* argv[])
{
	printf("factinject v0.1\n");
	printf("---------------\n");
	fflush(stdout);
	
  // Find the attachee process
	target_pid = find_pid_by_pattern ("^factorio$");
	if (!target_pid) {
		fprintf(stderr, "Coudln't attach to target process. Exiting.\n");
		fflush(stderr);
		return 1;
	}
	
	// Attach via ptrace
	printf("Attaching to process %d. ", target_pid);
	if (ptrace(PTRACE_SEIZE,target_pid,0,PTRACE_O_TRACESYSGOOD) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(1);
	}

	printf("Success.\n");
	fflush(stdout);

	// Remote break the tracee
	rbreak(target_pid);
  		
	// Inject self
	printf("Inject current executable into tracee.\n");
  inject_dlopen(target_pid, "/home/joya/localdev/factinject/factinject", RTLD_NOW | RTLD_GLOBAL);
	//inject_dlopen(target_pid, "libunwind.so", RTLD_NOW);
	printf("Done.\n");

	printf("Calling dlsym to get a pointer to test_func()\n");
	inject_dlsym(target_pid, "test_func");
	printf("Done.\n");
		
	// Continue tracee
	if (ptrace(PTRACE_CONT,target_pid,0,0) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(3);
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
}

char factinjectpath[] = "/home/joya/localdev/factinject/factinject";

void bootstrap_inject ()
{
	dlopen(factinjectpath, RTLD_NOW);
}
