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
#include <iostream>

#include "rbreak.h"
#include "Inject.hpp"

void cleanup ();

int main (int argc, char* argv[])
{
	printf("---------------\n");
	printf("factinject v0.2\n");
	printf("---------------\n");
	fflush(stdout);

	Tracee* factorio = nullptr;

  // Find the attachee "factorio"
	try {
		factorio = Tracee::FindByNamePattern("^factorio$");
	} catch (std::runtime_error e) {
		std::cerr << "Error finding factorio process:\n" << e.what();
	}
	
	// Attach via ptrace
	printf("Attaching to process %d. ", factorio->pid);
	factorio->Attach();
	
	printf("Success.\n");
	fflush(stdout);

	// Traps the tracee
	factorio->rbreak();
  		
	// Inject self
	printf("Inject current executable into tracee.\n");
	factorio->Inject_dlopen("/home/joya/localdev/factinject/factinject", RTLD_NOW | RTLD_GLOBAL);
	printf("Done.\n");

	printf("Continuing...\n");
	factorio->rcont();
	sleep(1);

	
	factorio->rbreak();
	printf("Calling dlerror\n");
	factorio->Inject_dlerror();
	printf("Done.\n");
	
		
	// Continue tracee
	factorio->rcont();
	
	while (1) {
		
	}
	
	// All done.
	delete factorio;
	
	return 0;
}


void test_func ()
{
	printf("Hello, PIE world!\n");
}


char factinjectpath[] = "/home/joya/localdev/factinject/factinject";

void bootstrap_inject ()
{
	dlopen(factinjectpath, RTLD_NOW);
}
