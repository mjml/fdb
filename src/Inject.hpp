#pragma once

#include <signal.h>
#include <vector>
#include <map>
#include <string>

#ifndef _INJECT_CPP
#define EXTERN extern
#else
#define EXTERN
#endif

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

struct SymbolTableEntry
{
	uint64_t    offset;
	char        type;   // man nm
	std::string name;
	SymbolTableEntry() : offset(0), type(0) {}
	void Parse (const char* line);
};


struct SymbolTable
{
	std::string path;
	std::map<std::string,SymbolTableEntry> symbols;

	SymbolTable () {}
	SymbolTable (const std::string& path);
	~SymbolTable () {}

	std::optional<uint64_t>            FindSymbolOffsetByPattern (const char* name_pat) const;
	std::optional<const std::string*>  FindSymbolNameByPattern (const char* name_pat) const;
	const SymbolTableEntry*            FindSymbolByPattern (const char* name_pat) const;
	
	void Parse (const std::string& _path);
	
};

struct SymbolTableMemo
{
	std::map<std::string, SymbolTable> tables;

	SymbolTableMemo () {}
	~SymbolTableMemo () {};

	void AddSymbolTable (const SymbolTable& table) { tables.insert(std::pair(table.path, table)); }
	void RemoveSymbolTable (const SymbolTable& table) { tables.erase(tables.find(table.path)); }
	const SymbolTable* FindSymbolTableByName (const std::string& pathname) const;
	const SymbolTable* FindSymbolTableByPattern (const char* regex) const;
};

EXTERN SymbolTableMemo symbolTableMemo;

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
	bool parsed_symtab;
	SymbolTable symtab;
	bool parsed_segtab;
	std::vector<SegInfo> segtab;
	
	Process () : pid(0), parsed_symtab(false), parsed_segtab(false), segtab(500) {}
	Process (int _pid) : pid(_pid), parsed_symtab(false), parsed_segtab(false), segtab(100) {}
	~Process () {}
	
	std::string FindExecutablePath ();
	
	void ParseSymbolTable ();
	
	void ParseSegmentMap ();
	
	uint64_t FindSymbolOffsetByPattern (const char* regex_pattern);

	uint64_t FindSymbolAddressByPattern (const char* regex_pattern);

	uint64_t FindSymbolAddressByPattern (const char* pat_symbol, const char* module);
	
	const SegInfo* FindSegmentByPattern (const char* regex_pattern, const char* flags);
	
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

	int GrabText (int size, void* addr, uint8_t* buffer);

	int InjectText (int size, void* addr, const uint8_t* buffer);

	int InjectAndRunText (int size, const uint8_t* text);
	
	void* Inject_dlopen (const char* szsharedlib, uint32_t flags);

	void* Inject_dlsym (const char* szsymbol);

	void* Inject_dlerror ();
	
};

#undef EXTERN
