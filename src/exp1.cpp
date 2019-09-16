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
	Tracee* tracee = Tracee::FindByNamePattern("^factorio$");
	std::cout << tracee->FindRemoteExecutablePath() << std::endl;
	std::cout << "lua_setglobal is at 0x" << std::hex << tracee->FindRemoteSymbolByPattern("lua_setglobal") << std::endl;
	
	return 0;
}
