#define _INJECT_CPP
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <regex>

#include "util/exceptions.hpp"
#include "factinject_logger.hpp"
#include "Inject.hpp"

static void print_siginfo (siginfo_t* ps)
{
	Logger::info("siginfo.si_signo = %d   siginfo.si_code = %d   siginfo.si_errno = %d",
							 ps->si_signo, ps->si_code, ps->si_errno);
}


void SymbolTableEntry::Parse (const char* line)
{
	char szname[1024];
	int c = sscanf(line, "%lx %c %1023s", &offset, &type, &szname);
	if (c == 3) {
		name = std::string(szname);
	}
}


SymbolTable::SymbolTable (const std::string& _path) 
	: path(_path)
{
	Parse(path);
};


void SymbolTable::Parse (const std::string& _path)
{
	char cmd[1024];
	
	symbols.clear();
	
	snprintf(cmd, 1024, "nm %s", _path.c_str());
	FILE* cmdout = popen(cmd, "r");

	int r = 0;
	char* line = nullptr;
	size_t n = 0;
	
	for (int i=0; i < 3; i++) {
		r = getline(&line, &n, cmdout);
		free(line);
		line = nullptr;
	}
	
	SymbolTableEntry entry;
	do {
		n = 0;
		r = getline(&line, &n, cmdout);
		if (r == 0) {
			free(line);
			line = nullptr;
			continue;
		}
		entry.type = 0;
		entry.Parse(line);
		if (entry.type) {
			symbols.emplace(entry.name, entry);
		}
		free(line);
		line = nullptr;
		
	} while (r >= 0);
	
	pclose(cmdout);
	
	path = _path;

	symbolTableMemo.AddSymbolTable(*this);
}


std::optional<uint64_t> SymbolTable::FindSymbolOffsetByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return std::optional(uint64_t(it->second.offset));
		}
	}
	return std::optional<uint64_t>();
}


std::optional<const std::string*> SymbolTable::FindSymbolNameByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return std::optional(&it->second.name);
		}
	}
	return std::optional<const std::string*>();
}


const SymbolTableEntry* SymbolTable::FindSymbolByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return &it->second;
		}
	}
	return nullptr;
}


const SymbolTable*  SymbolTableMemo::FindSymbolTableByName (const std::string& pathname) const
{
	for (auto it = tables.begin(); it != tables.end(); it++) {
		const std::string& s = it->first;
		if (s == pathname) {
			return &(it->second);
		}
	}
	return nullptr;
}


const SymbolTable*  SymbolTableMemo::FindSymbolTableByPattern (const char* regex_pattern) const
{
	std::regex reg(regex_pattern);
	for (auto it = tables.begin(); it != tables.end(); it++) {
		const std::string& s = it->first;
		if (regex_search(s, reg)) {
			return &(it->second);
		}
	}
	return nullptr;
}


std::string Process::FindExecutablePath ()
{
	assert (pid);
	char exelink[1024];
	snprintf(exelink, 1024, "/proc/%d/exe", pid);
	char exepath[1024];
	memset(exepath, 0, 1024);
	readlink(exelink, exepath, 1024);
	return std::string(exepath);
}


void Process::ParseSymbolTable ()
{
	std::string executable_path = FindExecutablePath();
	if (!parsed_symtab) {
		const SymbolTable* memoized = symbolTableMemo.FindSymbolTableByName(executable_path);
		if (memoized) {
			symtab = *memoized;
		} else {
			symtab.Parse(executable_path);
		}
	}
	parsed_symtab = true;
}

void Process::ParseSegmentMap ()
{
	char mapsfn[1024];
	char line[2048];
	char name[1024];
	snprintf(mapsfn,1024,"/proc/%d/maps", pid);
	
	std::ifstream mapsfile (mapsfn);
	assert_re (mapsfile.good(), "Couldn't open %s", mapsfn);
	
	segtab.clear();
	
	SegInfo segi;
	do {
		
		mapsfile.getline(line,2047);
		name[0] = 0;
		int c = sscanf(line, "%lx - %lx %s %x %x:%x %d %1023s",
					 &segi.start,
					 &segi.end,
					 &segi.flags,
					 &segi.offset,
					 &segi.maj,
					 &segi.min,
					 &segi.inode,
					 name
					 );
		if (c >= 8) {
			segi.file = std::string(name);
			segtab.emplace_back(segi);
		}
		
	} while (!mapsfile.eof());
	
}


uint64_t Process::FindSymbolOffsetByPattern (const char* regex_pattern)
{
	char cmd[1024];
	uint64_t result = 0L;

	if (!parsed_symtab) {
		ParseSymbolTable();
	}
	
	symtab.FindSymbolOffsetByPattern(regex_pattern);

	return result;
}

uint64_t Process::FindSymbolAddressByPattern (const char* sym_pat)
{
	uint64_t offset = FindSymbolOffsetByPattern (sym_pat);
	const SegInfo* seg = FindSegmentByPattern(symtab.path.c_str(), "--x-");
	
	return seg->start + offset;
}

uint64_t Process::FindSymbolAddressByPattern (const char* module_pat, const char* sym_pat)
{
	const SegInfo* seg = FindSegmentByPattern(module_pat, "--x-");
	assert_re(seg, "Couldn't find segment pattern '%s' in the target process!", module_pat);
	const std::string& pathname = seg->file;
	const SymbolTable* symtab = symbolTableMemo.FindSymbolTableByName(pathname);
	SymbolTable mysymtab;
	if (symtab == nullptr) {
		mysymtab.Parse(pathname.c_str());
		symtab = &mysymtab;
	}
	auto symofs = symtab->FindSymbolOffsetByPattern(sym_pat);
	assert_re(symofs.has_value(), "Couldn't find symbol offset for pattern '%s'", sym_pat);
	Logger::debug("segment is 0x%lx offset is 0x%lx", seg->start, symofs.value());
	
	return seg->start + symofs.value();	
}


const SegInfo* Process::FindSegmentByPattern (const char* regex_pattern, const char* flags)
{
	if (!parsed_segtab) {
		ParseSegmentMap();
		parsed_segtab = true;
	}
	std::regex reg(regex_pattern);
	
	for (auto it = segtab.begin(); it != segtab.end(); it++) {
		SegInfo& segi = *it;
		for (int i=0; i <= 4; i++) {
			if (flags[i] != '-' && flags[i] != segi.flags[i]) {
				goto keepgoing;
			}
		}
		if (regex_search(segi.file, reg)) {
			return &segi;
		}

	keepgoing:
		asm("nop;");
	}
	
	return nullptr;
}


Tracee* Tracee::FindByNamePattern (const char* regex_pattern)
{
	int result=0;
	regex_t regex;
	regcomp (&regex, regex_pattern, REG_EXTENDED);
	
	DIR* procdir = opendir("/proc");
	struct dirent* de = readdir(procdir);
	while (de) {
		int pid = atoi(de->d_name);
		if (!pid) {
			de = readdir(procdir);
			continue;
		}

		char cmdpathfn[1024];
		snprintf(cmdpathfn, 1024, "/proc/%d/comm", pid);

		std::fstream fs(cmdpathfn, std::ios::in);
		std::string str;
		fs >> str;

		if (regexec(&regex, str.c_str(), 0, nullptr, 0) == 0) {
			result = pid;
			fs.close();
			break;
		}
		
		fs.close();
		de = readdir(procdir);
	}
	
	regfree(&regex);

	assert_re(result, "couldn't find process using the pattern '%s'", regex_pattern);
	
	return new Tracee(result);
}


void Tracee::Attach ()
{
	if (!pid) {
		char msg[1024];
		snprintf(msg,1024, "Error: no pid defined to attach to", strerror(errno));
		throw std::runtime_error(msg);
	}
	
	if (ptrace(PTRACE_SEIZE,pid,0,PTRACE_O_TRACESYSGOOD) == -1) {
		char msg[1024];
		snprintf(msg,1024, "Error attaching to process: %s", strerror(errno));
		throw std::runtime_error(msg);
	}
}

int Tracee::SaveRegisters (struct user* ur)
{
	if (ptrace(PTRACE_GETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_GETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	return 0;

}


int Tracee::RestoreRegisters (struct user* ur)
{
	if (ptrace(PTRACE_SETREGS, pid, 0, &ur->regs) == -1) {
		return 1;
	}

	if (ptrace(PTRACE_SETFPREGS, pid, 0, &ur->i387) == -1) {
		return 2;
	}
	
	return 0;
}


int Tracee::rbreak ()
{

	// Send interrupt
	//Logger::info("Tracee::rbreak(): sending interrupt to %d.", pid);
	if (ptrace(PTRACE_INTERRUPT,pid,0,0) == -1) {
		Logger::error("Error : %s", strerror(errno));
		exit(2);
	}
	
	int wstatus;
	siginfo_t siginfo;
	struct user ur;

	int syscallstops = 0;
	
	do {
		
		waitpid(pid, &wstatus,0);
		//Logger::detail("Stopped: ");

		SaveRegisters(&ur);
		
		ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo);
		if (WIFSTOPPED(wstatus)) {
			//Logger::detail("# STOP received. rip=0x%lx  rbp=0x%lx  rsp=0x%lx", ur.regs.rip, ur.regs.rbp, ur.regs.rsp);
			//Logger::detail("siginfo.si_info is 0x%lx", siginfo.si_code);
			if (WSTOPSIG(wstatus) == (SIGTRAP|0x80)) {
				// this indicates that we're inside a syscall-enter-stop
				syscallstops++;
				//Logger::detail("wstatus == SIGTRAP|0x80");
			} else if (WSTOPSIG(wstatus) == SIGTRAP) {
				//syscallstops++;
				//Logger::detail("wstatus == SIGTRAP");
				// this indicates that we're inside a syscall-exit stop
			} else {
				//Logger::detail("WSTOPSIG(wstatus) == 0x%lx", WSTOPSIG(wstatus));
			}
			if (syscallstops < 2 || ur.regs.rbp < 0x1000) {
				ptrace(PTRACE_SYSCALL,pid,0,0);
				continue;
			} else {
				break;
			}
		} else {
			// exit or termination
			Logger::error("Signal received by debugger but tracee is not stopped. Exiting.");
			return 1;
		}

		asm("nop;");

	} while (1);

	return 0;

}

int Tracee::rcont ()
{
	return ptrace(PTRACE_CONT, pid, 0, 0);
}


int Tracee::GrabText (int size, void* addr, uint8_t* buffer)
{
	assert (size % sizeof(long) == 0);

	for (int i=0; i < size; i += sizeof(long)) {
		long peeked = ptrace(PTRACE_PEEKTEXT, pid, (uint64_t)(addr)+i, NULL);
		if (peeked == -1) {
			return 1;
		}
		*(long*)(buffer+i) = peeked;
	}
	
	return 0;
	
}


int Tracee::InjectText (int size, void* rip, const uint8_t* text)
{
	// Note that 'text' is a pointer to codetext,
	// but that what POKETEXT wants is an actual _value_ passed as its second parameter, but cast as a void*.
	// So you have to convert the text pointer to a uint64_t pointer and dereference it to get the uint64_t,
	//    then treat that as (void*).
	
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		if (ptrace(PTRACE_POKETEXT, pid, (uint64_t)rip+i, (void*)*(uint64_t*)(text+i) ) == -1) {
			return 1;
		}
	}
	
	return 0;
}



int Tracee::InjectAndRunText (int size, const uint8_t* text)
{
  int r = 0;
	struct user regset;

	/* save state */
	if (SaveRegisters(&regset)) {
		return 1;
	}

	void* rip = (void*)regset.regs.rip;
	uint8_t* savebuf = (uint8_t*)alloca(size);
	memset(savebuf, 0, size);

	if (GrabText(size, rip, savebuf)) {
		return 2;
	}

	/* copy code into remote process at IP and run it */
	if (InjectText(size, rip, text)) {
		return 3;
	}
	
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		return 4;
	}
	
	/* Wait for the program to issue SIGSTOP to itself
	 * (the text stub must end with a signal).
   * If the program doesn't SIGSTOP then we end up waiting forever!
	 */
	siginfo_t siginfo;
	do {
		int result = waitid(P_PID, pid, &siginfo, WSTOPPED);
		if (result == -1) {
			return 5;
		}
		if (siginfo.si_code == CLD_STOPPED) {
			break;
		}
		if (siginfo.si_code == CLD_EXITED ||
				siginfo.si_code == CLD_KILLED ||
				siginfo.si_code == CLD_DUMPED) {
			return 6;
		}
	} while(siginfo.si_code != CLD_STOPPED);

	if (InjectText(size, (void*)regset.regs.rip, savebuf)) {
		return 7;
	}

	if (RestoreRegisters(&regset)) {
		return 8;
	}
		
	return 0;

}


void* Tracee::Inject_dlopen (const char* shlib_path, uint32_t flags)
{
	const int arg2_offset = 1;
	const int arg1_offset = 32;
	const int funcaddr_offset = 14;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	unsigned long segaddr;
	unsigned long symofs;
	uint8_t r;
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\xbe\x02\x00\x00\x00"                      // mov    $0x2,%esi
				 "\x48\x8d\x3d\x14\x00\x00\x00"              // lea    0x14(%rip),%rdi
				 "\x48\xb8\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 25);
	strncpy((char*)buffer + arg1_offset, shlib_path, bufsz-arg1_offset);
	*(int32_t*)(buffer+arg2_offset) = flags;
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	uint64_t symaddr = FindSymbolAddressByPattern("/libdl", "__dlopen");
	
	*((uint64_t*)(buffer+funcaddr_offset)) = symaddr;
	Logger::detail("dlopen is located at 0x%lx", symaddr);
	
	// save remote registers and existing rip text
	if (r = SaveRegisters(&ur)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		Logger::error("Problem grabbing text: %d", r);
		return (void*)2;
	}
	
	// inject the blob
	Logger::print("Injecting codetext...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		Logger::error("Couldn't continue the tracee.");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	Logger::info("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		Logger::info("Stopped. WSTOPSIG(wstatus) = %d", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		//print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = SaveRegisters(&ur2)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)4;
	}

	Logger::detail("rax value: 0x%lx", ur2.regs.rax);
	if (ur2.regs.rax == 0) {
		Logger::warning("dlopen FAILED");
	}
	Logger::detail("rip difference: 0x%lx - %lx = %lu", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	Logger::info("Restoring previous codetext");
	if (r = InjectText(bufsz, rip, saved)) {
		Logger::error("Problem injecting saved codetext: %d", r);
		return (void*)5;
	}

	Logger::info("Restoring previous registers");
	if (r =	RestoreRegisters (&ur)) {
		Logger::error("Problem restoring saved registers: %d", r);
		return (void*)6;
	}
	
	return (void*)ur2.regs.rax;

}

void* Tracee::Inject_dlsym (const char* szsymbol)
{
	const int arg1_offset = 32;
  const int funcaddr_offset = 9;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	uint8_t r;

	// find dlsym's address and name
	auto seg = FindSegmentByPattern("/libdl", "--x-");
	assert_re(seg, "Couldn't find libdl segment in process space.");
	const SymbolTable* symtab = symbolTableMemo.FindSymbolTableByName(seg->file);
	SymbolTable mysymtab;
	if (!symtab) {
		mysymtab.Parse(seg->file);
		symtab = &mysymtab;
	}
	auto symbol = symtab->FindSymbolByPattern("__dlsym");
	assert_re(symbol, "Couldn't find symbol offset for dlsym()");
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\x48\x8d\x3d\x20\x00\x00\x00"              // lea    0x20(%rip),%rdi
				 "\x48\xb8\x01\x02\x03\x04\x05\x06\x07\x08"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 20);
	strncpy((char*)((uint8_t*)buffer + arg1_offset), symbol->name.c_str(), bufsz-arg1_offset);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	*((uint64_t*)(buffer+funcaddr_offset)) = seg->start + symbol->offset;
	Logger::info("dlsym (%s) is located at 0x%lx", symbol->name, seg->start + symbol->offset);
	
	// save remote registers and existing rip text
	if (r = SaveRegisters(&ur)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		Logger::error("Problem grabbing text: %d", r);
		return (void*)2;
	}

	// inject the blob
	Logger::info("Injecting codetext...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		Logger::error("Couldn't continue the tracee.");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	Logger::info("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		Logger::info("Stopped. WSTOPSIG(wstatus) = %d", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = SaveRegisters(&ur2)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)4;
	}

	Logger::detail("rax value: 0x%lx", ur2.regs.rax);
	Logger::detail("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	Logger::detail("Restoring previous codetext");
	if (r = InjectText(bufsz, rip, saved)) {
		Logger::error("Problem injecting saved codetext: %d", r);
		return (void*)5;
	}
	
	Logger::detail("Restoring previous registers");
	if (r =	RestoreRegisters (&ur)) {
		Logger::error("Problem restoring saved registers: %d", r);
		return (void*)6;
	}

	return (void*)ur2.regs.rax;
}

void* Tracee::Inject_dlerror ()
{
	const int arg1_offset = 32;
  const int funcaddr_offset = 2;
	const int bufsz = 512;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	struct user ur;
	void *rip, *rip2;
	void *rbp;
	uint8_t r;
	
	// find dlsym's address and name
	uint64_t symaddr = FindSymbolAddressByPattern("/libdl", "__dlerror");
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\x48\xb8\x01\x02\x03\x04\x05\x06\x07\x08"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 13);
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	*((uint64_t*)(buffer+funcaddr_offset)) = symaddr;
	Logger::info("dlerror is located at 0x%lx", symaddr);
	
	// save remote registers and existing rip text
	if (r = SaveRegisters(&ur)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)1;
	}
	rip = (void*)ur.regs.rip;
	if (r = GrabText (bufsz, rip, saved)) {
		Logger::error("Problem grabbing text: %d", r);
		return (void*)2;
	}

	// inject the blob
	Logger::detail("Injecting codetext...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		Logger::error("Couldn't continue the tracee.\n");
		return (void*)3;
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	Logger::detail("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		Logger::detail("Stopped. WSTOPSIG(wstatus) = %d", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
		//print_siginfo(&siginfo);
	}
	
	// check the return code
	struct user ur2;
	if (r = SaveRegisters(&ur2)) {
		Logger::error("Problem saving remote registers: %d", r);
		return (void*)4;
	}

	uint64_t result = ur2.regs.rax;
	Logger::debug("rax value: 0x%lx", ur2.regs.rax);
	Logger::debug("rip difference: 0x%lx - %lx = %lu", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// peek the block of text that contains the error into a big buffer
	char dlerrmsg[512];
	if (result) {
		GrabText(512,(void*)(result),(uint8_t*)(dlerrmsg));
		dlerrmsg[511] = 0;
		Logger::warning("dlerror returned: %s", dlerrmsg);
	}

	// restore tracee rip text and registers
	Logger::detail("Restoring previous codetext");
	if (r = InjectText(bufsz, rip, saved)) {
		Logger::error("Problem injecting saved codetext: %d", r);
		return (void*)5;
	}

	Logger::detail("Restoring previous registers");
	if (r =	RestoreRegisters (&ur)) {
		Logger::error("Problem restoring saved registers: %d", r);
		return (void*)6;
	}

	Logger::info("Continuing tracee...");
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		Logger::error("Couldn't continue the tracee after running code injection.");
		return (void*)7;
	}

	return (void*)ur2.regs.rax;
}

void Tracee::Kill (int signal)
{
	kill(pid, signal);
}

