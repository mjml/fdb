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

const char stdioname[] = "stdio";
const char mainlogfilename[] = "logfile";
const char applogname[] = "factinject";

template class Log<100,stdioname,FILE>;
template class Log<100,mainlogfilename,FILE>;
template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;

void hook_lua_gc (BreakpointCtx& ctx);

FILE** pfh = &StdioSink::file;

int main (int argc, char* argv[])
{
	StdioSink::initialize_with_handle (stdout);
	MainLogFileSink::initialize_with_filename("/tmp/factinject/log");

	Logger::print("Startup.");
	
	Tracee* factorio = nullptr;
	SymbolTable precache1;
	SymbolTable precache2;
	SymbolTable precache3;
	precache1.Parse("/usr/lib64/libc-2.27.so");
	precache2.Parse("/usr/lib64/libpthread-2.27.so");
	precache3.Parse("/usr/lib64/ld-2.27.so");
	
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
	factorio->Continue();
	
	// Call dlerror
	if (!soaddr) {
		factorio->Break();
		Logger::print("Calling dlerror");
		factorio->Inject_dlerror();
		Logger::print("Done.");
		factorio->Continue();
		return 0;
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
	Logger::print("Waiting for lua to start. In Factorio, start or load a new game from the menu.");
	WaitResult wr;
	while (1) {
		
		try {
			int stopsignal = 0;
			wr = factorio->Wait<0,0,false>(&stopsignal);
			if (wr == WaitResult::Stopped && stopsignal == SIGTRAP) {
				Logger::info("Caught stop signal %d", stopsignal);
				factorio->DispatchAtStop();
			} else if (wr == WaitResult::Trapped && stopsignal == SIGTRAP) {
				Logger::info("Caught trap signal %d", stopsignal);
				factorio->DispatchAtStop();
			}
			
			
		} catch (const std::exception e) {
			Logger::error("Main loop std::exception: %s", e.what());
			break;
		} catch (...) {
			Logger::error("Unidentified exception occured");
			break;
		}
		if (wr == WaitResult::Exited || wr == WaitResult::Terminated || wr == WaitResult::Faulted) {
			break;
		}
		
		Logger::info("Continuing.");
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

void hook_lua_gc (BreakpointCtx& ctx)
{
	struct user ur;
	ctx.tracee.SaveRegisters(&ur);
	uint64_t rdi = ur.regs.rdi;
	lua_State* ps = reinterpret_cast<lua_State*> (rdi);
	
	if (luastates.find(ps) == luastates.end()) {
		Logger::print("Found new lua_State 0x%lx", ps);
		luastates.insert(ps);
	}
	ctx.disabled = true;
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
