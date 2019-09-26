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


uint64_t SymbolTable::FindSymbolOffsetByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return uint64_t(it->second.offset);
		}
	}
	Throw(std::runtime_error, "Couldn't find symbol offset using pattern '%s'", name_pat);
}


const std::string* SymbolTable::FindSymbolNameByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return &it->second.name;
		}
	}
	Throw(std::runtime_error, "Couldn't find symbol matching name '%s'", name_pat);
}


const SymbolTableEntry* SymbolTable::FindSymbolByPattern (const char* name_pat) const
{
	std::regex reg(name_pat);
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		if (std::regex_search(it->first, reg)) {
			return &it->second;
		}
	}
	Throw(std::runtime_error, "Couldn't find symbol");
}


auto SymbolTableMemo::FindSymbolTableByName (const std::string& pathname) const -> result_type
{
	for (auto it = tables.begin(); it != tables.end(); it++) {
		const std::string& s = it->first;
		if (s == pathname) {
			return std::optional(&(it->second));
		}
	}
	return result_type();
}


auto SymbolTableMemo::FindSymbolTableByPattern (const char* module_pat) const -> result_type
{
	std::regex reg(module_pat);
	for (auto it = tables.begin(); it != tables.end(); it++) {
		const std::string& s = it->first;
		if (regex_search(s, reg)) {
			return std::optional<const SymbolTable*>(&(it->second));
		}
	}
	return result_type();
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
		auto memoized = symbolTableMemo.FindSymbolTableByName(executable_path);
		if (memoized.has_value()) {
			symtab = *(memoized.value());
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


uint64_t Process::FindSymbolOffsetByPattern (const char* sym_pat)
{
	char cmd[1024];

	if (!parsed_symtab) {
		ParseSymbolTable();
	}
	
	auto result = symtab.FindSymbolOffsetByPattern(sym_pat);
	
	return result;
}

uint64_t Process::FindSymbolAddressByPattern (const char* sym_pat)
{
	auto symofs = FindSymbolOffsetByPattern (sym_pat);
	//auto seg = FindSegmentByPattern(symtab.path.c_str(), "--x-");

	// symbols within the local executable (not a shared library) have 
	//return seg->start + symofs;
	return symofs;
}

uint64_t Process::FindSymbolAddressByPattern (const char* module_pat, const char* sym_pat)
{
	auto seg = FindSegmentByPattern(module_pat, "--x-");
	const std::string& pathname = seg->file;
	auto symtab = symbolTableMemo.FindSymbolTableByName(pathname);
	SymbolTable mysymtab; 
	if (!symtab.has_value()) {
		mysymtab.Parse(pathname.c_str());
		symtab = &mysymtab;
	}
	auto symofs = (*symtab)->FindSymbolOffsetByPattern(sym_pat);
	Logger::debug("segment is 0x%lx  offset is 0x%lx", seg->start, symofs);
	
	return seg->start + symofs;
}


const SegInfo* Process::FindSegmentByPattern (const char* module_pat, const char* flags)
{
	if (!parsed_segtab) {
		ParseSegmentMap();
		parsed_segtab = true;
	}
	std::regex reg(module_pat);
	
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
	
	Throw(std::runtime_error, "Couldn't find a segment with a module module matching '%s'", module_pat);
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
	assert_re(pid, "Error: no pid defined to attach to");
	
	if (ptrace(PTRACE_SEIZE,pid,0,PTRACE_O_TRACESYSGOOD) == -1) {
		throw PTraceException(errno);
	}
}

void Tracee::SaveRegisters (struct user* ur)
{
	int r = 0;
	if (r = ptrace(PTRACE_GETREGS, pid, 0, &ur->regs); r == -1) {
		throw PTraceException(errno);
	}

	if (r = ptrace(PTRACE_GETFPREGS, pid, 0, &ur->i387); r == -1) {
		throw PTraceException(errno);
	}
}


void Tracee::RestoreRegisters (struct user* ur)
{
	int r = 0;
	if (r = ptrace(PTRACE_SETREGS, pid, 0, &ur->regs); r  == -1) {
		throw PTraceException(errno);
	}

	if (r = ptrace(PTRACE_SETFPREGS, pid, 0, &ur->i387); r == -1) {
		throw PTraceException(errno);
	}
}

constexpr static char logname[] = "Break";
typedef Log<3,logname,Logger> RBLog;

void Tracee::Break ()
{
	int r = 0;

	RBLog::info("Tracee::Break(): sending interrupt to %d.", pid);
	if (r = ptrace(PTRACE_INTERRUPT,pid,0,0); r  == -1) {
		RBLog::error("Error : %s", strerror(errno));
		throw PTraceException(errno);
	}
	
	int wstatus;
	siginfo_t siginfo;
	struct user ur;

	int syscallstops = 0;
	
	do {
		
		waitpid(pid, &wstatus,0);
		RBLog::detail("Stopped: ");
		
		SaveRegisters(&ur);
		
		ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo);
		if (WIFSTOPPED(wstatus)) {
			RBLog::detail("# STOP received. rip=0x%lx  rbp=0x%lx  rsp=0x%lx", ur.regs.rip, ur.regs.rbp, ur.regs.rsp);
			RBLog::detail("siginfo.si_info is 0x%lx", siginfo.si_code);
			if (WSTOPSIG(wstatus) == (SIGTRAP|0x80)) {
				// this indicates that we're inside a syscall-enter-stop
				syscallstops++;
				RBLog::detail("wstatus == SIGTRAP|0x80");
			} else if (WSTOPSIG(wstatus) == SIGTRAP) {
				//syscallstops++;
				RBLog::detail("wstatus == SIGTRAP");
				// this indicates that we're inside a syscall-exit stop
			} else {
				RBLog::detail("WSTOPSIG(wstatus) == 0x%lx", WSTOPSIG(wstatus));
			}
			if (syscallstops < 2 || ur.regs.rbp < 0x1000) {
				ptrace(PTRACE_SYSCALL,pid,0,0);
				continue;
			} else {
				break;
			}
		} else {
			// exit or termination
			const char *msg = "Signal received by debugger but tracee is not stopped. Exiting.";
			RBLog::error(msg);
			Throw(std::runtime_error, msg);
		}
		
		asm("nop;");

	} while (1);
}


void Tracee::AsyncContinue ()
{
	if (long r = ptrace(PTRACE_CONT, pid, 0, 0); r == -1 ) {
		throw PTraceException(errno);
	}
}


void Tracee::Continue ()
{
	if (long r = ptrace(PTRACE_CONT, pid, 0, 0); r == -1) {
		throw PTraceException(errno);
	}
}


void Tracee::GrabText (int size, uint64_t addr, uint8_t* buffer)
{
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		long r = ptrace(PTRACE_PEEKTEXT, pid, addr+i, NULL);
		if (r == -1) {
			throw std::runtime_error("Unexpected -1 returned from ptrace(PTRACE_PEEKTEXT, ...)");
		}
		*(long*)(buffer+i) = r;
	}
}


void Tracee::InjectText (int size, uint64_t rip, const uint8_t* text)
{
	// Note that 'text' is a pointer to codetext,
	// but that what POKETEXT wants is an actual _value_ passed as its second parameter, but cast as a void*.
	// So you have to convert the text pointer to a uint64_t pointer and dereference it to get the uint64_t,
	//    then treat that as (void*).
	
	assert (size % sizeof(long) == 0);
	
	for (int i=0; i < size; i += sizeof(long)) {
		if (long result = ptrace(PTRACE_POKETEXT, pid, rip+i, (void*)*(uint64_t*)(text+i) ); result == -1) {
			throw PTraceException(errno);
		}
	}
}



void Tracee::InjectAndRunText (int size, const uint8_t* text)
{
  long r = 0;
	struct user regset;

	/* save state */
	SaveRegisters(&regset);

	void* rip = (void*)regset.regs.rip;
	uint8_t* savebuf = (uint8_t*)alloca(size);
	memset(savebuf, 0, size);
	GrabText(size, rip, savebuf);

	/* copy code into remote process at IP and run it */
	InjectText(size, rip, text);
	
	if (r = ptrace(PTRACE_CONT, pid, 0, 0); r == -1) {
		throw PTraceException(errno);
	}
	
	/* Wait for the program to issue SIGSTOP to itself
	 * (the text stub must end with a signal).
   * If the program doesn't SIGSTOP then we end up waiting forever!
	 */
	siginfo_t siginfo;
	do {
		if (int result = waitid(P_PID, pid, &siginfo, WSTOPPED); result == -1) {
			errno_exception(std::runtime_error);
		}
		
		if (siginfo.si_code == CLD_STOPPED) {
			break;
		}
		assert_e<std::runtime_error> (siginfo.si_code != CLD_EXITED &&
																	siginfo.si_code != CLD_KILLED &&
																	siginfo.si_code != CLD_DUMPED,
																	"Unrecognized signal 0x%x detected after stop", siginfo.si_code);
	} while (siginfo.si_code != CLD_STOPPED);

	InjectText(size, (void*)regset.regs.rip, savebuf);
	RestoreRegisters(&regset);

}


void Tracee::RegisterBreakpoint (PBreakpoint& brkp)
{
	auto addr = brkp->addr;
	brkpts.insert(std::pair(addr, brkp));
}

void Tracee::UnregisterBreakpoint (PBreakpoint& brkp)
{
	brkpts.erase(brkp->addr);
}


void Tracee::EnableBreakpoint (PBreakpoint& brkp)
{
	const int bufsiz = 128;
	uint8_t buf[bufsiz];
	memset(buf, 0, bufsiz);
	Logger::print("Enabling breakpoint at 0x%lx", brkp->addr);
	
	GrabText(bufsiz, brkp->addr, buf);
	GrabText(sizeof(uint64_t), brkp->addr, reinterpret_cast<uint8_t*>(&brkp->replaced));
	uint64_t opbrk = (brkp->replaced & ~(uint64_t)(0xff)) | 0xcc;
	InjectText(sizeof(uint64_t), brkp->addr, reinterpret_cast<uint8_t*>(&opbrk));
	brkp->enabled = true;
	GrabText(bufsiz, brkp->addr, buf);
}


void Tracee::DisableBreakpoint (PBreakpoint& brkp)
{
	assert_re(brkp->enabled, "Disabling a breakpoint that is already disabled can lead to erroneus code injection.");
	Logger::info("Disabling breakpoint");
	InjectText(sizeof(uint64_t), reinterpret_cast<void*>(brkp->addr), reinterpret_cast<uint8_t*>(&brkp->replaced));
	brkp->enabled = false;
}


void Tracee::Wait (int* stop_signal)
{
	int wstatus;
	siginfo_t siginfo;
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		int sig = WSTOPSIG(wstatus);
		Logger::info("Tracee stopped.");
		Logger::detail("WSTOPSIG(wstatus) = %d", sig);
		if (stop_signal) *stop_signal = sig;
		if (long result = ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo); result == -1) {
			Logger::error("Error while getting signal info from tracee");
			throw PTraceException(errno);
		}
	} 
}


void Tracee::DispatchAtStop ()
{
	// get the ip
	struct user ur;
	uint64_t rip = 0;
	uint64_t rdi = 0;
	SaveRegisters(&ur);
	rip = ur.regs.rip - 1; // -1 to move back past the 0xcc stop instruction

	if (auto srch = brkpts.find(rip); srch != brkpts.end()) {
		
		PBreakpoint &brkp = srch->second;
		assert_re (brkp->enabled, "Stopped at breakpoint that is not enabled!");

		(*brkp)(*this);
		
		// now replace the code that was missing, back up the rip, single-step through the replacement, then re-enable 
		DisableBreakpoint(brkp);
		ur.regs.rip--;
		RestoreRegisters(&ur);

		long r = ptrace(PTRACE_SINGLESTEP, pid, nullptr, 0);
		if (r == -1) {
			throw PTraceException(errno);
		}
		
		EnableBreakpoint(brkp);
		
	} else {
		Logger::warning("Breakpoint hit at 0x%lx, but no matching breakpoint found!", ur.regs.rip);
	}
}


auto Tracee::Inject_dlopen (const char* shlib_path, uint32_t flags) -> pointer
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
	SaveRegisters (&ur);
	
	rip = (void*)ur.regs.rip;
	GrabText (bufsz, rip, saved);
	
	// inject the blob
	Logger::print("Injecting dlopen call...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (long result = ptrace(PTRACE_CONT, pid, 0, 0); result == -1) {
		Logger::error("Couldn't continue the tracee.");
		throw PTraceException(errno);
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	Logger::info("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		Logger::info("Stopped. WSTOPSIG(wstatus) = %d", WSTOPSIG(wstatus));
		if (long result = ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo); result == -1) {
			Logger::error("Error while getting signal info from tracee");
			throw PTraceException(errno);
		}
	}
	
	// check the return code
	struct user ur2;
	SaveRegisters(&ur2);

	Logger::info("dlopen returned: 0x%lx", ur2.regs.rax);
	if (ur2.regs.rax == 0) {
		Logger::warning("dlopen FAILED");
	}
	Logger::detail("rip difference: 0x%lx - %lx = %lu", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	Logger::info("Restoring previous codetext");
	InjectText(bufsz, rip, saved);

	Logger::info("Restoring previous registers");
	RestoreRegisters (&ur);
	
	return ur2.regs.rax;

}

auto Tracee::Inject_dlsym (const char* szsymbol) -> pointer
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
	auto symtab = symbolTableMemo.FindSymbolTableByName(seg->file);
	SymbolTable mysymtab;
	if (!symtab.has_value()) {
		mysymtab.Parse(seg->file);
		symtab = &mysymtab;
	}
	auto symbol = (*symtab)->FindSymbolByPattern("__dlsym");
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
	SaveRegisters(&ur);
	
	rip = (void*)ur.regs.rip;
	GrabText (bufsz, rip, saved);

	// inject the blob
	Logger::info("Injecting codetext...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (long result = ptrace(PTRACE_CONT, pid, 0, 0); result == -1) {
		Logger::error("Couldn't continue the tracee.");
		throw PTraceException(result);
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
	SaveRegisters(&ur2);
	Logger::detail("rax value: 0x%lx", ur2.regs.rax);
	Logger::detail("rip difference: 0x%lx - %lx = %lu\n", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// restore tracee rip text and registers
	Logger::info("Restoring previous codetext");
	InjectText(bufsz, rip, saved);
	
	Logger::info("Restoring previous registers");
	RestoreRegisters (&ur);
	
	return ur2.regs.rax;
}


auto Tracee::Inject_dlerror () -> pointer
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
	SaveRegisters(&ur);
	rip = reinterpret_cast<void*>(ur.regs.rip);
	GrabText (bufsz, rip, saved);

	// inject the blob
	Logger::detail("Injecting codetext...");
	InjectText (bufsz, rip, buffer);
	
	// continue the tracee
	if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
		throw PTraceException(errno);
	}

	// wait for the tracee to stop
	int wstatus;
	siginfo_t siginfo;
	Logger::detail("Waiting for completion...");
	waitpid(pid, &wstatus, 0);
	if (WIFSTOPPED(wstatus)) {
		Logger::detail("Stopped. WSTOPSIG(wstatus) = %d", WSTOPSIG(wstatus));
		ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo);
	}
	
	// check the return code
	struct user ur2; 
	SaveRegisters(&ur2);

	uint64_t result = ur2.regs.rax;
	Logger::debug("rax value: 0x%lx", ur2.regs.rax);
	Logger::debug("rip difference: 0x%lx - %lx = %lu", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	// peek the block of text that contains the error into a big buffer
	if (result) {
		char dlerrmsg[512];
		GrabText(512,(void*)(result),(uint8_t*)(dlerrmsg));
		dlerrmsg[511] = 0;
		Logger::warning("dlerror returned: %s", dlerrmsg);
	}
	
	// restore tracee rip text and registers
	Logger::detail("Restoring previous codetext");
	InjectText(bufsz, rip, saved);

	Logger::detail("Restoring previous registers");
	RestoreRegisters (&ur);

	return ur2.regs.rax;
}

void Tracee::Kill (int signal)
{
	kill(pid, signal);
}

