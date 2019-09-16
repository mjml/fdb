#pragma once

#include <signal.h>
#include <string>

struct Tracee
{
	int pid;

	Tracee () : pid(0) {}
	Tracee (int _pid) : pid(_pid) {}
	~Tracee () {}
	
	static Tracee* FindByNamePattern (const char* regex_pattern);

	void Attach ();
	
	void Kill (int signal=SIGKILL);

	int rbreak ();

	int rcont ();

	std::string FindRemoteExecutablePath ();
	
	uint64_t FindRemoteSymbolByPattern (const char* regex_pattern);
	
	int SaveRemoteRegisters (struct user* ur);

	int RestoreRemoteRegisters (struct user* ur);

	int GrabText (int size, void* rip, uint8_t* buffer);

	int InjectText (int size, void* rip, const uint8_t* buffer);

	int InjectAndRunText (int size, const uint8_t* text);
	
	void* Inject_dlopen (const char* szsharedlib, uint32_t flags);

	void* Inject_dlsym (const char* szsymbol);

	void* Inject_dlerror ();

	
	
};

