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

#include <set>
#include "util/log.hpp"
#include "util/exceptions.hpp"
#include "factinject_logger.hpp"
#include "Inject.hpp"

typedef void lua_State;

std::set<lua_State*> luastates;

template class Log<100,stdioname,FILE>;
template class Log<100,mainlogfilename,FILE>;
template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;

const char stdioname[] = "stdio";
const char mainlogfilename[] = "logfile";
const char applogname[] = "factinject";

void hook_lua_gc (Tracee& tracee,  Breakpoint& brkpt);

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
	Logger::print("Done.");
	
	// Inject self
	Logger::print("Calling dlopen to inject.");
	factorio->Break();
	Tracee::pointer soaddr = factorio->Inject_dlopen("/home/joya/localdev/factinject/src/factinject", RTLD_NOW | RTLD_GLOBAL);
	Logger::print("Done.");
	factorio->AsyncContinue();
	
	// Call dlerror
	if (!soaddr) {
		factorio->Break();
		Logger::print("Calling dlerror");
		factorio->Inject_dlerror();
		Logger::print("Done.");
		factorio->AsyncContinue();
	}
	
	// Trap a Lua function in order to find Lua_state*
	uint64_t lua_gc_addr = factorio->FindSymbolAddressByPattern ("^lua_pushinteger$");
	Logger::info("Found lua_pushinteger address at 0x%lx", lua_gc_addr);
	auto brkp = std::make_shared<Breakpoint>(hook_lua_gc);
	brkp->addr = lua_gc_addr;

	factorio->RegisterBreakpoint(brkp);
	factorio->Break();
	factorio->EnableBreakpoint(brkp);
	factorio->Continue();
	
	// spinwait
	while (1) {
		
		try {
			Logger::print("Waiting for breakpoint...");
			int stopsignal = 0;
			factorio->Wait(&stopsignal);
			Logger::print("Program break");
			if (stopsignal == SIGTRAP) {
				factorio->DispatchAtStop();
			} else {
				Logger::error ("Tracee received signal %d", stopsignal);
			}
			
		} catch (const std::exception e) {
			Logger::error("Main loop std::exception: %s", e.what());
		} catch (...) {
			Logger::error("Unidentified exception occured");
			break;
		}
		factorio->Continue();
	}
	
	// All done. (never?)
	Logger::print("Terminating.");
	delete factorio;

	Logger::finalize();
	MainLogFileSink::finalize();
	StdioSink::finalize();
	
	return 0;
}

void hook_lua_gc (Tracee& tracee, Breakpoint& brkpt)
{
	struct user ur;
	tracee.SaveRegisters(&ur);
	uint64_t rdi = ur.regs.rdi;
	lua_State* ps = reinterpret_cast<lua_State*> (rdi);
	
	if (luastates.find(ps) == luastates.end()) {
		Logger::print("Found new lua_State 0x%lx", ps);
		luastates.insert(ps);
	}
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
