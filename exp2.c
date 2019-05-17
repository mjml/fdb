#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>

int main (int argc, char* argv[])
{

	for (int i=0; 1; i++) {
		i &= 0xf;
		if (!(i & 4)) {
			printf(".");
		} else {
			printf("\b \b");
		}
		fflush(stdout);
		sleep(1);
	}

	return 0;
}


int never_called ()
{
	printf("This function was invoked remotely!\n");

	void *h = dlopen("libunwind.so", RTLD_GLOBAL);

	if (h) {
		printf("dlopen() call successful\n");
		return 5;
	} else {
		printf("dlopen() call failed\n");
		return 3;
	}
	
}
