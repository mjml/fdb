#define _FACTINJECT_CPP

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

#ifndef LOGLEVEL_FACTINJECT
#define LOGLEVEL_FACTINJECT 6
#endif

#include "util/log.hpp"
#include "util/exceptions.hpp"
#include "factinject_logger.hpp"
#include "Inject.hpp"

void cleanup ();

FILE** pfh = &StdioSink::file;

int main (int argc, char* argv[])
{
	StdioSink::initialize_with_handle (stdout);
	MainLogFileSink::initialize_with_filename("/tmp/factinject/log");

	Logger::print("Startup.");
	
	Tracee* factorio = nullptr;

  // Find the attachee "factorio"
	try {
		factorio = Tracee::FindByNamePattern("^factorio$");
	} catch (std::runtime_error e) {
		Logger::error("Error finding factorio process: %s", e.what());
		fprintf(stderr, "Couldn't find factorio, sorry!\n");
		fflush(stderr);
		std::exit(1);
	}
	
	// Attach via ptrace
	Logger::print("Attaching to process %d.", factorio->pid);
	factorio->Attach();
	Logger::print("Success.");
	
	// Inject self
	Logger::info("Inject current executable into tracee.");
	factorio->rbreak();
	factorio->Inject_dlopen("/home/joya/localdev/factinject/src/factinject", RTLD_NOW | RTLD_GLOBAL);
	Logger::info("Done.");
	factorio->rcont();
	sleep(1);
	
	// Call dlerror
	factorio->rbreak();
	Logger::detail("Calling dlerror");
	factorio->Inject_dlerror();
	Logger::detail("Done.");
	factorio->rcont();
	
	// Trap a Lua function in order to find Lua_state*
	/*
	factorio->rbreak();
	factorio->ParseSymbolTable();
	factorio->ParseSegmentMap();
	factorio->rcont();
	*/
	
	// spinwait
	while (1) {
		
	}
	
	// All done.
	delete factorio;

	Logger::finalize();
	MainLogFileSink::finalize();
	StdioSink::finalize();
	
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
