#pragma once

#include <signal.h>
#include <time.h>
#include <sys/ptrace.h>
#include <list>
#include <map>
#include <string>
#include <memory>
#include <functional>

#ifndef _INJECT_CPP
#define EXTERN extern
#else
#define EXTERN
#endif


struct Tracee;

struct PTraceException : public std::exception
{
	constexpr static const char msg_EBUSY[] = "There was an error with allocating or freeing a debug register.";
	
  constexpr static const char msg_EFAULT[] = "There  was  an attempt to read from or write to an invalid area in the tracer's "
		"or the tracee's memory, probably because the area wasn't mapped or accessible. "
		"Unfortunately, under Linux, different variations of "
		"this fault will return EIO or EFAULT more or less arbitrarily.";

  constexpr static const char msg_EINVAL[] = "An attempt was made to set an invalid option.";

  constexpr static const char msg_EIO[] = "Request is invalid, or an attempt was made to read from or write to an "
		"invalid area in the tracer's or the tracee's memory, or there was a word-alignment "
		"violation, or an invalid signal was specified during a restart request.";

  constexpr static const char msg_EPERM[] = "The specified process cannot be traced. This could be because the tracer has "
		"insufficient privileges (the required capability is CAP_SYS_PTRACE); unprivileged "
		"processes cannot trace processes that they cannot send signals to or those running "
		"set-user-ID/set-group-ID programs, for obvious reasons.  Alternatively, the process "
		"may already be being traced, or (on kernels before 2.6.26) be init(1) (PID 1).";

  constexpr static const char msg_ESRCH[] = "The specified process does not exist, or is not currently being traced by the caller, "
		"or is not stopped (for requests that require a stopped tracee).";
	
	long retval;

	const char* what() {
		switch(retval) {
		case EBUSY: return msg_EBUSY;
		case EFAULT: return msg_EFAULT;
		case EINVAL: return msg_EINVAL;
		case EIO: return msg_EIO;
		case EPERM: return msg_EPERM;
		}
	}

	PTraceException (int r) : retval(r) {}
	~PTraceException () = default;
};

struct BreakpointCtx;

struct Breakpoint
{
	uint64_t   addr;
	bool       enabled;
	uint64_t   replaced;
	int        cnt;
	std::function<void(BreakpointCtx& ctx)> handler;
	
	Breakpoint (void (*f)(BreakpointCtx&) ) : addr(0), enabled(false), replaced(0), cnt(0), handler(f) {}
	~Breakpoint ()  = default;
	
	virtual void operator() (BreakpointCtx& ctx) {
		cnt++;
		handler(ctx);
	}
};
typedef std::shared_ptr<Breakpoint> PBreakpoint;

struct BreakpointCtx
{
	Tracee& tracee;
	PBreakpoint brkp;
	bool disabled;
	BreakpointCtx (Tracee& t, PBreakpoint& b) : tracee(t), brkp(b), disabled(false) {}
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

	uint64_t            FindSymbolOffsetByPattern (const char* name_pat) const;
	const std::string*  FindSymbolNameByPattern (const char* name_pat) const;

	const SymbolTableEntry*  FindSymbolByPattern (const char* name_pat) const;
	
	void Parse (const std::string& _path);
	
};


struct SymbolTableMemo
{
	typedef std::optional<const SymbolTable*> result_type;
	
	std::map<std::string, SymbolTable> tables;

	SymbolTableMemo () {}
	~SymbolTableMemo () {};

	void AddSymbolTable (const SymbolTable& table) { tables.insert(std::pair(table.path, table)); }
	void RemoveSymbolTable (const SymbolTable& table) { tables.erase(tables.find(table.path)); }
	result_type FindSymbolTableByName (const std::string& pathname) const;
	result_type FindSymbolTableByPattern (const char* regex) const;
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
	std::list<SegInfo> segtab;
	
	Process () : pid(0), parsed_symtab(false), parsed_segtab(false), segtab() {}
	Process (int _pid) : pid(_pid), parsed_symtab(false), parsed_segtab(false), segtab() {}
	~Process () {}
	
	std::string FindExecutablePath ();
	
	void ParseSymbolTable ();
	
	void ParseSegmentMap ();
	
	uint64_t FindSymbolOffsetByPattern (const char* regex_pattern);
	
	uint64_t FindSymbolAddressByPattern (const char* regex_pattern);
	
	uint64_t FindSymbolAddressByPattern (const char* pat_symbol, const char* module);
	
	const SegInfo* FindSegmentByPattern (const char* regex_pattern, const char* flags);
	
};

enum WaitResult { Unknown,
									Interrupted,
									Continued,
									Stopped,
									Trapped,
									Faulted,
									Exited,
									Terminated
};



struct Tracee : public Process
{
	typedef uint64_t pointer;
	
	Tracee () : Process() {}
	Tracee (int _pid) : Process(_pid) {}
	~Tracee () {}
	
	std::map< uint64_t, PBreakpoint > brkpts;
	
	static Tracee* FindByNamePattern (const char* name_pat);
	static int FindPidByNamePAttern (const char* name_pat);
	
	void Attach ();
	
	void Kill (int signal=SIGKILL);
	
	void Break ();
	
	void AsyncContinue ();

	void Continue ();
	
	void SaveRegisters (struct user* ur);
	
	void RestoreRegisters (struct user* ur);

	void GrabText (int size, void* addr, uint8_t* buffer) { GrabText(size, reinterpret_cast<uint64_t>(addr), buffer); }

	void GrabText (int size, uint64_t addr, uint8_t* buffer);
	
	void InjectText (int size, void* addr, const uint8_t* buffer) { InjectText(size, reinterpret_cast<uint64_t>(addr), buffer); }
	
	void InjectText (int size, uint64_t addr, const uint8_t* buffer);
	
	void InjectAndRunText (int size, const uint8_t* text);
	
	void RegisterBreakpoint (PBreakpoint& brkp);
	
	void UnregisterBreakpoint (PBreakpoint& brkp);
	
	void EnableBreakpoint (PBreakpoint& brkp);
	
	void DisableBreakpoint (PBreakpoint& brkp);

	template<int Sec=0, int USec=0, bool ThrowOnFailure=true>
	WaitResult Wait (int* stop_signal = nullptr);
	
  void DispatchAtStop ();
	
	pointer Inject_dlopen (const char* szsharedlib, uint32_t flags);
	
	pointer Inject_dlsym (const char* szsymbol);
	
	pointer Inject_dlerror ();
	
protected:
	WaitResult _Wait (int* stop_signal = nullptr);

	
};

template <int Sec=0, int USec=0, bool ThrowOnFailure=true>
WaitResult Tracee::Wait (int* stop_signal = nullptr)
{
	timer_t tmr;
	struct sigevent sev;
	struct itimerspec ts;

	memset(&tmr, 0, sizeof(timer_t));
	memset(&sev, 0, sizeof(struct sigevent));
	
	sev.sigev_value.sival_ptr = reinterpret_cast<void*>(&tmr);
	sev.sigev_signo = SIGALRM;
  sev.sigev_notify = SIGEV_SIGNAL;
	
	if (int r = timer_create(CLOCK_REALTIME, &sev, &tmr); r != 0) {
		Throw(std::runtime_error,"Couldn't create realtime clock for timed wait.");
	}

	ts.it_value.tv_sec = Sec;
	ts.it_value.tv_nsec = USec * 1000;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	if (int r = timer_settime(tmr, 0, &ts, 0); r == -1) {
		Throw(std::runtime_error,"Couldn't arm timed wait timer.");
	}

	WaitResult result;
	if (ThrowOnFailure) {
		std::exception_ptr pe;
		try {
			result = Wait<0,0,true>(stop_signal);
		} catch (...) {
			pe = std::current_exception();
		}
		if (int r = timer_delete(tmr); r) {
			Throw(std::runtime_error,"Couldn't delete timed wait timer.");
		}
		if (pe) {
			std::rethrow_exception(pe);
		}
		return result;
		
	} else {
		result = Wait<0,0,false>(stop_signal);
	}
	
	return result;	
}


template <>
WaitResult Tracee::Wait<0,0,false> (int* stop_signal = nullptr)
{
	int wstatus;
	siginfo_t siginfo;
	
	int retval = waitpid(pid, &wstatus, 0);
	if (retval == ECHILD) {
		// pid is not a process or isn't traced
		return WaitResult::Unknown;
	} else if (retval == EINTR) {
		// a signal was sent to the current process
		return WaitResult::Interrupted;
	}
	
	if (WIFSTOPPED(wstatus)) {
		int sig = WSTOPSIG(wstatus);
		bool syscall = sig >> 7 ? true : false;
		Logger::detail("Tracee stopped. WSTOPSIG(wstatus) = %d  syscall=%s", sig, syscall ? "true" : "false");
		if (stop_signal) *stop_signal = sig;
		if (long result = ptrace(PTRACE_GETSIGINFO,pid,0,&siginfo); result == -1) {
			Logger::error("Error while getting signal info from tracee");
			throw PTraceException(errno);
		}
		Logger::debug("siginfo:   .si_signo=%d   .si_errno=%d   .si_code=%d",
									 siginfo.si_signo, siginfo.si_errno, siginfo.si_code);

		assert_re(siginfo.si_code == sig, "siginfo.si_code != sig");
		if (sig == 5) {
			return WaitResult::Trapped;
		} else if (sig==SIGSEGV || sig==SIGFPE || sig==SIGILL || sig==SIGABRT || sig==SIGPIPE ) {
			return WaitResult::Faulted;
		} else {
			return WaitResult::Stopped;
		}
		
	} else if (WIFCONTINUED(wstatus)) {
		return WaitResult::Continued;
	} else if (WIFEXITED(wstatus)) {
		if (ThrowOnFailure) {
			
		} else {
			if (stop_signal) { *stop_signal = wstatus & 0xff; } // exit code
			return WaitResult::Exited;
		}
	} else if (WIFSIGNALED(wstatus)) {
		if (stop_signal) { *stop_signal = WTERMSIG(wstatus); }
		return WaitResult::Terminated;
	} else {
		const char* msg = "Unidentified program status after waitpid().";
		Logger::error(msg);
		Throw(std::runtime_error,msg);
	}

}


template<typename...F>
struct CTTracee : public Tracee
{
	std::tuple<F...> funcs;
	
	CTTracee(F..._f) : Tracee(), funcs(_f...) {}
	~CTTracee();
	
	void Wait();
};

#undef EXTERN

