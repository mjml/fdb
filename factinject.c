#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "findpid.h"

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
	if (ptrace(PTRACE_SEIZE,target_pid,0,0) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(1);
	}
	printf("Success.\n");


	siginfo_t siginfo;
	memset(&siginfo, 0, sizeof(siginfo_t));
	waitid(P_PID, target_pid, &siginfo, WSTOPPED);
	
	cleanup();
	
	return 0;
}


void test_func ()
{
	printf("Hello, PIE world!\n");
}


void cleanup()
{
	ptrace(PTRACE_CONT,target_pid,0,18);
}

void bootstrap_inject ()
{
	dlopen("/home/joya/localdev/factinject/factinject", RTLD_NOW);
}
