#include <dlfcn.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>

#include <iostream>
#include "Inject.hpp"

void init_dlfcns()
{
	void* h = dlopen("this should return zero", RTLD_NOW);
	assert(h == NULL);
	
}


int main ()
{
	Tracee* tracee = nullptr;
	try {
		 tracee = Tracee::FindByNamePattern("^factorio$");
	} catch (...) {
		std::cerr << "factorio doesn't seem to be running." << std::endl;
		std::exit(1);
	}
	
	std::cout << tracee->FindExecutablePath() << std::endl;
	
	std::cout << "lua_setglobal offset is at 0x" << std::hex << tracee->FindSymbolOffsetByPattern("lua_setglobal") << std::endl;
	
	return 0;
}
