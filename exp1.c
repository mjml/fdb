#include <dlfcn.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>

void init_dlfcns()
{
	void* h = dlopen("this should return zero", RTLD_NOW);
	assert(h == NULL);
	
}


int main () {

	init_dlfcns();
	int i=0;
	
	while (1) {
		if (i & 0x1) {
			printf(".");
			fflush(stdout);
		} else {
			printf("\b \b");
			fflush(stdout);
		}
		sleep(1);
		i++;
	}

	return 0;
}
