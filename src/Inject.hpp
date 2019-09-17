#pragma once

#include <signal.h>
#include <vector>
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

	SymbolTable () {}
	SymbolTable (std::string& path);
	~SymbolTable ();

	uint64_t FindSymbolOffsetByPattern (const char* regex_pattern);

	void Parse (const std::string& _path);
	
};

struct SegInfo
{
	uint64_t start;
	uint64_t end;
	char flags[5];
	int offset;
	char maj;
	char min;
	uint32_t inode;
	std::string file;

	SegInfo()
		: start(0),
			end(0),
			flags{0,0,0,0,0},
			offset(0),
			maj(0),
			min(0),
			inode(0),
			file() {}
	~SegInfo() {}
};


struct Process
{
	int pid;
	bool parsed_symtable;
	SymbolTable symtable;
	bool parsed_segments;
	std::vector<SegInfo> segtable;
	
	Process () : pid(0), parsed_symtable(false), segtable(500) {}
	Process (int _pid) : pid(_pid), parsed_symtable(false), segtable(500) {}
	~Process () {}
	
	std::string FindExecutablePath ();
	
	uint64_t FindSymbolOffsetByPattern (const char* regex_pattern);
	
	uint64_t FindSegmentByPattern (const char* regex_pattern, char* flags);
	
	void ParseSegments ();
	
};

struct Tracee : public Process
{

	Tracee () : Process() {}
	Tracee (int _pid) : Process(_pid) {}
	~Tracee () {}
	
	static Tracee* FindByNamePattern (const char* regex_pattern);

	void Attach ();
	
	void Kill (int signal=SIGKILL);

	int rbreak ();

	int rcont ();

	int SaveRegisters (struct user* ur);

	int RestoreRegisters (struct user* ur);

	int GrabText (int size, void* rip, uint8_t* buffer);

	int InjectText (int size, void* rip, const uint8_t* buffer);

	int InjectAndRunText (int size, const uint8_t* text);
	
	void* Inject_dlopen (const char* szsharedlib, uint32_t flags);

	void* Inject_dlsym (const char* szsymbol);

	void* Inject_dlerror ();
	
};

