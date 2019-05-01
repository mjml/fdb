
- Start Factorio

- Run injector from command line

- Injector attaches ptrace to Factorio's parent thread (do the other threads in the thread group also stop?)

- Injector checks /proc/PID/maps to get the address of libdl in the Factorio process

- Injector attaches stub somewhere to call dlopen on a user provided shared library

- Injector attaches stub somewhere to call dlsym (repeatedly) to obtain addresses for the .so's exported symbols within the Factorio process

- lua mod code calls code that is known to call the trapped function, along with other parameters

- conditional breakpoint code catches mod arguments and passes them to injector, which now knows the address of lua_State

- injector uses saveregs/poketext/restoreregs to inject code that will call lua_register with the injected lua_CFunction and the discovered lua_State


