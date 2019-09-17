#pragma once

#include <signal.h>
#include <map>
#include <string>

struct Tracee;

struct Breakpoint
{
	Tracee*   tracee;
	bool      enabled;
	uint64_t  r_addr;
	uint8_t   savedbyte;
	
	Breakpoint(Tracee* _tracee, bool _enabled, uint64_t _r_addr);
	~Breakpoint();
	
	void Enable ();
	void Disable ();
	
};


struct BreakpointMgr
{
	static std::map<uint64_t, Breakpoint> bpmap;

	static void Wait ();
	
	static Breakpoint& FindBreakpoint (uint64_t rip);
};


struct SymbolTable
{
	std::string path;
	std::map<std::string,int> offsets;
	
	SymbolTable (std::string& path);
	~SymbolTable ();

	uint64_t FindSymbolOffsetByPattern (const char* regex_pattern);

	void Parse (const std::string& _path);
	
};


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
	
	uint64_t FindRemoteSymbolOffsetByPattern (const char* regex_pattern);
	
	int SaveRemoteRegisters (struct user* ur);

	int RestoreRemoteRegisters (struct user* ur);

	int GrabText (int size, void* rip, uint8_t* buffer);

	int InjectText (int size, void* rip, const uint8_t* buffer);

	int InjectAndRunText (int size, const uint8_t* text);
	
	void* Inject_dlopen (const char* szsharedlib, uint32_t flags);

	void* Inject_dlsym (const char* szsymbol);

	void* Inject_dlerror ();

	
	
};

