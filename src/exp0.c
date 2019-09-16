#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>


static int this_var_is_static = 1;

int main (int argc, char* argv[])
{
	printf("Trying to open factinject as a shared library. Does its main() get run?\n");
	
	void* hlib = dlopen("./factinject", RTLD_NOW);
	if (!hlib) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}

	printf("Success: 0x%08x\n", hlib);
		
	return 0;
}
