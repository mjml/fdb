
- Start Factorio

- Run injector from command line

- Injector attaches ptrace to Factorio's parent thread (do the other threads in the thread group also stop? - I think they do.)

- Injector waitpid()'s for Factorio to stop

- Injector checks /proc/PID/maps to get the address of libdl in the Factorio process

- Injector gets the addresses of dlopen, dlsym, dlclose (possibly from calling nm on the libdl.so itself)

- How to call dlopen / dlsym?
  - It just needs the address of these symbols, which it can get with an offset from the beginning of their mapped pages (?)
	  - Essentially, /proc/PID/maps and output from nm or readelf
	- Basically, it needs to provide its own relocations for whatever imports it wants to call

- How to inject a stub created from a local function?
	 - That function needs its own relocatables (compile with -fpie)
	    in order to fixup the addresses of its calls to shared library functions, and any other external data (.got)
	- Need the start and end address of the function that it's trying to inject so that fixups can be done within this range.

- Injector injects a stub to call dlopen on a user provided shared library

- Injector injects a stub to call dlsym (repeatedly) to obtain addresses for the .so's exported symbols within the Factorio process

- How to call lua_register?
  - How to obtain the lua_State?
	  - "Trap" (conditional breakpoint) a lua call that is known to be called by a widget with specific parameters
		- When the call is made, record the address of the lua_State that was passed in
		- Untrap the lua call
	- 

- Injector injects a stub to call lua_register with the lua_State and the function(s) of the newly imported shared library

- What if we try to build an executable with relocatable code that can be injected into a target process?
  - linker options: -pie -static-pie (-E --export-dynamic)
	- compiler options: -fpie
  - need to ensure that the injected local function has been resolved and/or the plt has been bypassed so that its true address can be found in the GOT.
