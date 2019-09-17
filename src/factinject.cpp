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

struct factinject_logger_traits
{
	constexpr static const char* name = "factinject";
	constexpr static int logLevel = LOGLEVEL_FACTINJECT;
};

typedef Log<factinject_logger_traits> Logger;

int main (int argc, char* argv[])
{
	Logger::initialize();

	Logger::print("Startup.");
	fflush(stdout);
	
	Tracee* factorio = nullptr;

  // Find the attachee "factorio"
	try {
		factorio = Tracee::FindByNamePattern("^factorio$");
	} catch (std::runtime_error e) {
		Logger::error("Error finding factorio process: %s", e.what());
	}
	
	// Attach via ptrace
	Logger::print("Attaching to process %d.", factorio->pid);
	factorio->Attach();
	
	Logger::print("Success.\n");
	fflush(stdout);
	
	// Inject self
	Logger::info("Inject current executable into tracee.");
	factorio->rbreak();
	factorio->Inject_dlopen("/home/joya/localdev/factinject/factinject", RTLD_NOW | RTLD_GLOBAL);
	Logger::info("Done.");
	factorio->rcont();
	sleep(1);

	// Call dlerror
	factorio->rbreak();
	Logger::detail("Calling dlerror");
	factorio->Inject_dlerror();
	Logger::detail("Done.");
	factorio->rcont();

	// Trap a Lua function in order to find Lua_context*

	// spinwait
	while (1) {
		
	}
	
	// All done.
	delete factorio;

	log::finalize();
	
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