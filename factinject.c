#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <sys/ptrace.h>

#include "findpid.h"

int target_pid;

void cleanup ();

int main (int argc, char* argv[])
{
	
  // Find the attachee process
	target_pid = find_pid_by_pattern ("^factorio$");
	if (!target_pid) {
		fprintf(stderr, "Coudln't attach to target process. Exiting.\n");
		fflush(stderr);
		return 1;
	}

	// Attach via ptrace
	printf("Attaching to process %d. ", target_pid);
	if (ptrace(PTRACE_ATTACH,target_pid,0,0) == -1) {
		fprintf(stderr, "Error : %s\n", strerror(errno));
		exit(1);
	}
	printf("Success.\n");

	
	
	cleanup();
	
	return 0;
	
}


void cleanup()
{
	ptrace(PTRACE_CONT,target_pid,0,18);
}
