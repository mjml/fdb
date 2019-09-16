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

#define APPNAME "factinject"

#include "util/log.hpp"
#include "Inject.hpp"

#ifndef LOGLEVEL_FACTINJECT
#define LOGLEVEL_FACTINJECT 5
#endif

void cleanup ();

struct factorio_logger_traits
{
	constexpr static const char* name = "factinject";
	constexpr static int logLevel = LOGLEVEL_FACTINJECT;
};

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
	
	// Inject self
	printf("Inject current executable into tracee.\n");
	factorio->rbreak();
	factorio->Inject_dlopen("/home/joya/localdev/factinject/factinject", RTLD_NOW | RTLD_GLOBAL);
	printf("Done.\n");
	factorio->rcont();
	sleep(1);

	// Call dlerror
	factorio->rbreak();
	printf("Calling dlerror\n");
	factorio->Inject_dlerror();
	printf("Done.\n");
	factorio->rcont();

	// spinwait
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
