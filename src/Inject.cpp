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

#include "util/microsleep.h"
#include "util/exceptions.hpp"
#include "factinject_logger.hpp"
#include "Inject.hpp"



const char waitlogname[] = "Tracee::Wait";
const char breaklogname[] = "Tracee::Break";

template class Log<LOGLEVEL_WAIT,waitlogname,Logger>;
typedef Log<LOGLEVEL_WAIT,waitlogname,Logger> WaitLog;

template class Log<LOGLEVEL_RB,breaklogname,Logger>;
typedef class Log<LOGLEVEL_RB,breaklogname,Logger> RBLog;

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

		SegInfo& segi2 = segtab.back();
		//Logger::debug2("Seginfo: %s [0x%lx - 0x%lx]",segi2.file.c_str(), segi2.start, segi2.end);
		
	} while (!mapsfile.eof());
	
	parsed_segtab = true;
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
	auto pair = FindSymbolAddressPairByPattern(module_pat, sym_pat);
	return pair.first + pair.second;
}

std::pair<uint64_t, uint64_t> Process::FindSymbolAddressPairByPattern (const char* module_pat, const char* sym_pat)
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
	
	return std::pair<uint64_t, uint64_t>(seg->start,symofs);
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
	
	Throw(std::runtime_error, "Couldn't find a segment with a module matching '%s'", module_pat);
}


std::pair<uint64_t, uint64_t> Process::DecomposeAddress (uint64_t address)
{
	SegInfo* found_seg = nullptr;
	for (auto it = segtab.begin(); it != segtab.end(); it++) {
		SegInfo& segi = *it;
		if (segi.start <= address && address < segi.end) {
			found_seg = &segi;
		}
	}
	// little hack that only uses segment address for shared libraries, but not the main executable segment
	uint64_t symofs = (address >= 0x700000000000) ? address - found_seg->start : address;
	if (found_seg) {
		// decomposition based on segment table
		return std::pair<uint64_t,uint64_t>(found_seg->start, symofs);
	} else {
		// generic page/offset decomposition
		return std::pair<uint64_t,uint64_t>(address & ~0xfff, address & 0xfff);
	}
}


std::pair<std::string, std::string> Process::DescribeAddress (uint64_t address)
{
	SegInfo* found_seg = nullptr;
	for (auto it = segtab.begin(); it != segtab.end(); it++) {
		SegInfo& segi = *it;
		if (segi.start <= address && address < segi.end) {
			found_seg = &segi;
		}
	}
	if (!found_seg) {
		return std::pair("[unknown]","<unknown>");
	}
	// little hack that only uses segment address for shared libraries, but not the main executable segment
	uint64_t symofs = (address >= 0x700000000000) ? address - found_seg->start : address;
	
	auto symtab = symbolTableMemo.FindSymbolTableByName(found_seg->file);
	SymbolTable mysymtab; 
	if (!symtab.has_value()) {
		mysymtab.Parse(found_seg->file.c_str());
		symtab = &mysymtab;
	}
	if (found_seg->file.c_str()[0] == '[') {
		return std::pair(found_seg->file, std::to_string(symofs));
	}

	auto& symbs = (*symtab)->symbols;
	int mindistance = INT_MAX;
	int distance = 0;
	SymbolTableEntry* found_entry = nullptr;
	std::string found_symname;
	for (auto it = symbs.begin(); it != symbs.end(); it++) {
		auto p = *it;
		SymbolTableEntry& entry = p.second;
		distance = symofs - entry.offset;
		if (distance > 0 && distance < mindistance) {
			//Logger::debug2("Found symbol %s at distance 0x%x", entry.name.c_str(), distance);
			mindistance = distance;
			found_symname = p.second.name;
		}
	}
	//Logger::debug2("Returning %s and %s", found_seg->file.c_str(), found_symname.c_str());
	char decorated_symname[512];
	snprintf(decorated_symname, 512, "%s+0x%lx", found_symname.c_str(), mindistance);
	return std::pair(found_seg->file, std::string(decorated_symname));
}


Tracee* Tracee::FindByNamePattern (const char* regex_pattern)
{
	int results = 0;
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
			results++;
			if (!result) {
				result = pid;
			}
		}
		
		fs.close();
		de = readdir(procdir);
	}

	
	
	closedir(procdir);
	
	regfree(&regex);
	
	assert_re(result != 0, "Couldn't find process using the pattern '%s'", regex_pattern);
	assert_re(results < 2, "Danger! Too many factorio processes running with pattern '%s'. Which one do I attach to?", regex_pattern);
	
	return new Tracee(result);
}


void Tracee::Attach ()
{
	assert_re(pid, "Error: no pid defined to attach to");
	ParseSegmentMap();
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

std::tuple<WaitResult, struct user> Tracee::Break ()
{
	int r = 0;

	RBLog::info("sending interrupt to %d.", pid);
	if (r = ptrace(PTRACE_INTERRUPT,pid,0,0); r  == -1) {
		RBLog::error("Error : %s", strerror(errno));
		throw PTraceException(errno);
	}
	
	int wstatus = 0;
	siginfo_t siginfo;
	struct user ur;
	WaitResult result = WaitResult::Unknown;
	bool syscall = true;
	int stopsig = 0;
	int sig = 0;
	uint64_t rbp = 0;
	int ptrace_event = 0;
	bool enter = false;
	bool again = true;

	do {
		RBLog::info("Waiting for %d...", pid);
		r = waitpid(pid, &wstatus,__WALL);
		RBLog::info("Wait complete.");

		if (r != pid) {
			const char* msg;
			if (r == EINTR) {
				result = WaitResult::Interrupted;
				Logger::warning("Break interrupted by signal");
				return std::tuple(result,ur); // ur is basically undefined
			} else if (r == ECHILD) {
				msg = "No such child exists: %d";
			  Logger::critical(msg,pid);
				Throw(std::runtime_error,msg,pid);
			} else  {
				msg = "Strange value %d returned by waitpid()";
				Logger::error(msg, r);
				Throw(std::runtime_error,msg,r);
			}
		}
		
		if (WIFSTOPPED(wstatus)) {
			
			// This is a ptrace-stop by definition.
			// It may be further classified as either signal-delivery-stop, group-stop, PTRACE_EVENT stop, or syscall-stop.

			stopsig = WSTOPSIG(wstatus);
			sig = stopsig & 0x7f;
			syscall = (stopsig >> 7) & 0x1;

			if (long result = ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo); result == -1) {
				Logger::error("Error while getting signal info from tracee");
				throw PTraceException(errno);
			}
			
			// If sig is SIGTRAP, then this should be a signal-delivery-stop or a syscall-stop
			SaveRegisters(&ur);
			
			RBLog::detail("STOPPED. rip=0x%-12lx rbp=0x%-12lx rsp=0x%-12lx stopsig=0x%-4x", ur.regs.rip, ur.regs.rbp, ur.regs.rsp, stopsig);
			
			bool extrabit1 = (siginfo.si_code >> 7) & 0x1;  // seems to agree with the syscall flag in WSTOPSIG(wstatus)
			ptrace_event = (siginfo.si_code >> 8);

			rbp = ur.regs.rbp;
			
			if (sig == SIGTRAP) {
				RBLog::debug("syscall=%s ptrace_event=0x%-2x", syscall?"true":"false", ptrace_event);
				if (syscall) {
					enter = ur.regs.rax == -ENOSYS;
					if (enter) {
						RBLog::debug("syscall-enter-stop");
					} else {
						RBLog::debug("syscall-exit-stop");
					}
				}
				result = WaitResult::Trapped;
			} else if (siginfo.si_code == SI_KERNEL) {
				// This SIGTRAP sent by the kernel (for unknown reasons)
				result = WaitResult::Stopped;
				RBLog::debug("SIGTRAP sent from kernel");
			} else if (siginfo.si_code <= 0) {
				// This is SIGTRAP delivered as a result of userspace action
				// such as tgkill, kill, sigqueue (maybe PTRACE?)
				result = WaitResult::Stopped;
				RBLog::debug("SIGTRAP sent by user process");
			} else {
				result = WaitResult::Stopped;
			}
		
			RBLog::debug("siginfo: .si_signo=%d   .si_errno=%d   .si_code=0x%x",
									 siginfo.si_signo, siginfo.si_errno, siginfo.si_code);
			
			
			if (RBLog::level > LogLevel::INFO) { // sanity check: what segment is rip in?
				bool found_segment = false;
				auto [ segofs, symofs ] = DecomposeAddress(ur.regs.rip);
				auto [ segname, symname ] = DescribeAddress(ur.regs.rip);
				RBLog::info("break landed in segment: %s(0x%lx) symbol: %s(0x%lx)", segname.c_str(), segofs, symname.c_str(), symofs);
			}

			if (ptrace_event == PTRACE_EVENT_STOP || rbp < 0x1000 || (syscall && enter)) { // heuristic for finding a "safe" breakpoint
				if (long result = ptrace(PTRACE_SYSCALL, pid, 0,0); result == -1) {
					throw PTraceException(errno);
				}
			} else {
				again = false;
			}
			
			
		} else if (WIFEXITED(wstatus)) {
			int exitcode = WEXITSTATUS(wstatus);
			const char *msg = "Program exited with code %d while tracer was waiting for it to interrupt.";
			RBLog::error(msg, exitcode);
			Throw(std::runtime_error, msg, exitcode);
		} else if (WIFSIGNALED(wstatus)) {
			int sig = WTERMSIG(wstatus);
			const char *msg = "Program terminated with signal %d while tracer was waiting for it to interrupt.";
			RBLog::error(msg, sig);
			Throw(std::runtime_error, msg, sig);
		} else if (WIFCONTINUED(wstatus)) {
			const char *msg = "Program mysteriously continued while tracer was waiting for it to interrupt.";
			RBLog::error(msg);
			Throw(std::runtime_error, msg);
		}
		
	} while (again);

	return std::tuple(result, ur);
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
	GrabText(sizeof(uint64_t), brkp->addr, reinterpret_cast<uint8_t*>(&brkp->replaced));
	uint64_t opbrk = (brkp->replaced & ~(uint64_t)(0xff)) | 0xcc;
	InjectText(sizeof(uint64_t), brkp->addr, reinterpret_cast<uint8_t*>(&opbrk));
	brkp->enabled = true;
}


void Tracee::DisableBreakpoint (PBreakpoint& brkp)
{
	Logger::info("Disabling breakpoint");
	InjectText(sizeof(uint64_t), brkp->addr, reinterpret_cast<uint8_t*>(&brkp->replaced));
	brkp->enabled = false;
}


WaitResult Tracee::_Wait (int* stop_signal)
{
	int wstatus;
	siginfo_t siginfo;
	
	int retval = waitpid(pid, &wstatus, 0);
	if (retval == ECHILD) {
		WaitLog::info("ECHILD received: pid %d is not a process or isn't traced", pid);
		return WaitResult::Unknown;
	} else if (retval == EINTR) {
		WaitLog::info("EINTR received: a signal was sent to the current process");
		return WaitResult::Interrupted;
	}
	
	if (WIFSTOPPED(wstatus)) {
		int stopsig = WSTOPSIG(wstatus);
		int sig = stopsig & 0x7f;
		bool syscall = sig >> 7 ? true : false;
		WaitLog::detail("Stopped: WSTOPSIG(wstatus)=%d  syscall=%s", sig, syscall ? "true" : "false");
		if (stop_signal) *stop_signal = sig;
		if (long result = ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo); result == -1) {
			WaitLog::error("Error while getting signal info from tracee");
			throw PTraceException(errno);
		}
		WaitLog::debug("siginfo .si_signo=%-4d .si_errno=%-4d .si_code=0x%-4x",
									 siginfo.si_signo, siginfo.si_errno, siginfo.si_code);

		
		if (sig == SIGTRAP) {
			WaitLog::info("trapped with sig==SIGTRAP(5)");
			return WaitResult::Trapped;
		} else if (sig==SIGSEGV || sig==SIGFPE || sig==SIGILL || sig==SIGABRT || sig==SIGPIPE ) {
			struct user ur;
			SaveRegisters(&ur);
			auto [ segname, symname ] = DescribeAddress(ur.regs.rip);
			WaitLog::info("faulted with sig=%d at rip=0x%lx  %s:%s",
										sig, ur.regs.rip, segname.c_str(), symname.c_str());
			return WaitResult::Faulted;
		} else {
			WaitLog::info("stopped with generic signal sig=%d", sig);
			return WaitResult::Stopped;
		}
		
	} else if (WIFCONTINUED(wstatus)) {
		WaitLog::info("continued");
		return WaitResult::Continued;
	} else if (WIFEXITED(wstatus)) {
		WaitLog::info("process exited");
		if (stop_signal) { *stop_signal = wstatus & 0xff; } // exit code
		return WaitResult::Exited;
	} else if (WIFSIGNALED(wstatus)) {
		WaitLog::info("process terminated with signal %d", WTERMSIG(wstatus));
		if (stop_signal) { *stop_signal = WTERMSIG(wstatus); }
		return WaitResult::Terminated;
	} else {
		const char* msg = "Unidentifiable program status after waitpid().";
		WaitLog::critical(msg);
		Throw(std::domain_error,msg);
	}
}


void Tracee::DispatchAtStop ()
{
	// get the ip
	struct user ur;
	uint64_t rip = 0;
	uint64_t rdi = 0;
	SaveRegisters(&ur);
	rip = ur.regs.rip - 1;     // -1 to move back past the 0xcc stop instruction
	bool user_disable = false; // true if the breakpoint handler flips the enabled bit to false
	
	if (auto srch = brkpts.find(rip); srch != brkpts.end()) {
		
		PBreakpoint &brkp = srch->second;
		assert_re (brkp->enabled, "Stopped at breakpoint that is not enabled!");
		
		BreakpointCtx ctx(*this, brkp);
		(*brkp)(ctx);
		
		DisableBreakpoint(brkp);
		ur.regs.rip--;
		RestoreRegisters(&ur);

		long r = ptrace(PTRACE_SINGLESTEP, pid, nullptr, 0);
		if (r == -1) {
			throw PTraceException(errno);
		}
		
		if (!ctx.disabled) {
			EnableBreakpoint(brkp);
		} else {
			Logger::debug("Keeping breakpoint disabled after singlestep at request of user.");
		}
		
	} else {
		Logger::warning("Breakpoint hit at 0x%lx, but no matching breakpoint found!", ur.regs.rip);
	}
}


auto Tracee::Inject_dlopen (const char* shlib_path, uint32_t flags) -> pointer
{
	const int arg2_offset = 8;
	const int arg1_offset = 40;
	const int funcaddr_offset = 22;
	const int bufsz = 256;
	uint8_t buffer[bufsz];
	uint8_t saved[bufsz];
	struct user ur;
	uint64_t rip;
	uint8_t r;
	
	// create the injected blob
	memset(buffer, 0, bufsz);
	memcpy(buffer, (uint8_t*)
				 "\x48\x81\xec\x00\x01\x00\x00"              // sub    $0x100, $rsp
				 "\xbe\x02\x00\x00\x00"                      // mov    $0x2,%esi
				 "\x48\x8d\x3d\x14\x00\x00\x00"              // lea    0x14(%rip),%rdi
				 "\x48\xb8\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8"  // mov    $0x0807060504030201, %rax
				 "\xff\xd0"                                  // call   *%rax
				 "\xcc"                                      // int    3
				 , 32);
	strncpy((char*)buffer + arg1_offset, shlib_path, bufsz-arg1_offset);
	*(int32_t*)(buffer+arg2_offset) = flags;
	
	// amend the blob with the sum of the segment address and symbol address of the target function
	uint64_t symaddr = FindSymbolAddressByPattern("/libdl", "__dlopen");
	
	*((uint64_t*)(buffer+funcaddr_offset)) = symaddr;
	Logger::detail("dlopen is located at 0x%lx", symaddr);
	
	SaveRegisters (&ur);
	
	rip = ur.regs.rip;
	GrabText (bufsz, rip, saved);
	
	Logger::print("Injecting dlopen call at 0x%lx...", rip);
	InjectText (bufsz, rip, buffer);
	
	Logger::print("Continuing.");
	Continue();

	Logger::print("Waiting for tracee to stop...");
	int sig;
	auto result = Wait(&sig);
	
	struct user ur2;
	SaveRegisters(&ur2);

	Logger::info("dlopen returned: 0x%lx", ur2.regs.rax);
	if (ur2.regs.rax == 0) {
		Logger::warning("dlopen FAILED");
	}
	Logger::detail("rip difference: 0x%lx - 0x%lx = %ld", ur2.regs.rip, ur.regs.rip, ur2.regs.rip - ur.regs.rip);
	
	Logger::info("Restoring previous codetext");
	InjectText(bufsz, rip, saved);

	Logger::info("Restoring previous registers");
	RestoreRegisters (&ur);
	
	return ur2.regs.rax; // this is the dlopen return value

}

auto Tracee::Inject_dlsym (const char* szsymbol) -> pointer
{
	const int arg1_offset = 32;
  const int funcaddr_offset = 9;
	const int bufsz = 256;
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
	Continue();

	// wait for the tracee to stop
	Wait();
	
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
	Continue();

	// wait for the tracee to stop
	Wait();
	
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

