#pragma once

#include <string>
#include <sstream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <cassert>
#include <stdexcept>
#include "util/errno_exception.hpp"


enum LogLevel {
								NONE = 0,
								CRITICAL, // An invariant is breached, or program state is jeopardized so that exit is required.
								ERROR,    // An invariant is breached, the current user operation/intent will fail, but the program can recover or resume.
								WARNING,  // An invariant may be breached, but program state is OK and the user operation will succeed.
								PRINT,    // Status messages that record nominal execution but that is neither voluminous nor proportional to input complexity.
								FUSS,     // Unexpected condition that is part of proper execution but may indicate improper usage by the user.
								INFO,     // Status messages that are not volumous in proportion to input complexity.
								DETAIL,   // Status messages that can be volumous in proportion to input complexity.
								DBG,    // Messages that are intended to show specific information with the intent of detecting preconditions to failure.
								DBG2    // The firehose.
};


template<int Level, const char* Name, typename...Sinks>
struct Log
{
	inline static int level = Level;
	
	static void write (const char* szstr, int size)
	{
		(Sinks::write(szstr,size), ...);
	}

	template<typename...Ps>
	static void fmtprint (int lvl, const char* fmt, Ps...ps);
	
public:
	static void initialize () {}

	static void finalize () {}

	template<int Lvl, typename...Ps> static void log_at_level (const char* fmt, Ps...ps);
	template<typename...Ps> static void debug2 (const char* fmt, Ps...ps) { log_at_level<LogLevel::DBG2,Ps...> (fmt,ps...); }
	template<typename...Ps> static void debug (const char* fmt, Ps...ps)  { log_at_level<LogLevel::DBG, Ps...> (fmt,ps...); }
	template<typename...Ps> static void detail (const char* fmt, Ps...ps) { log_at_level<LogLevel::DETAIL,Ps...> (fmt,ps...); }
	template<typename...Ps> static void info (const char* fmt, Ps...ps)   { log_at_level<LogLevel::INFO,Ps...> (fmt,ps...); }
  template<typename...Ps> static void print (const char* fmt, Ps...ps)  { log_at_level<LogLevel::PRINT,Ps...> (fmt,ps...); }
	template<typename...Ps>	static void fuss (const char* fmt, Ps...ps)   { log_at_level<LogLevel::FUSS,Ps...> (fmt,ps...); }
  template<typename...Ps> static void warning (const char* fmt, Ps...ps) { log_at_level<LogLevel::WARNING,Ps...> (fmt,ps...); }
	template<typename...Ps> static void error (const char* fmt, Ps...ps)  { log_at_level<LogLevel::ERROR,Ps...> (fmt,ps...); }
	template<typename...Ps> static void critical (const char* fmt, Ps...ps) { log_at_level<LogLevel::CRITICAL,Ps...> (fmt,ps...); }
	
};


template<int Level, const char* Name>
struct Log<Level,Name,FILE>
{
	inline static int level;
	inline static FILE* file;
	static void initialize_with_filename(const std::string& filename);
	static void initialize_with_handle(FILE* fh) { file = fh; }
	static void finalize();
	static void write (const char* szstr, int size);
};


template<int Level, const char* Name>
void Log<Level, Name, FILE>::initialize_with_filename (const std::string& filename)
{
	using namespace std;
	file = fopen(filename.c_str(), "w+");
	if (!file) { throw errno_exception(std::runtime_error); }
}


template<int Level, const char* Name>
void Log<Level, Name, FILE>::finalize ()
{
	assert(file != nullptr);
	if (!fclose(file)) { throw errno_exception(std::runtime_error); }
}


template<int Level, const char* Name>
inline void Log<Level, Name, FILE>::write (const char* szstr, int size)
{
	if (!fwrite(szstr, size, 1, file)) {
		throw errno_runtime_error;
	}
	if (!fputc('\n',file)) {
		throw errno_runtime_error;
	}
	if (fflush(file)) {
		throw errno_runtime_error;
	}
}


template<int Level, const char* Name, typename...Sinks>
template<typename...Ps>
inline void Log<Level, Name, Sinks...>::fmtprint (int lvl, const char* fmt, Ps...ps)
{
	char entry[1024];
	char msg[960];
	int chrs = snprintf(msg,960,fmt,ps...);
	
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &ts); 
	chrs = snprintf(entry, 1024,"%ld.%06ld [%s-%d] %s", ts.tv_sec, ts.tv_nsec/1000, Name, lvl, msg);
	
	write(entry,chrs);
}

template<int Level, const char* Name, typename...Sinks>
template<int Lvl, typename...Ps>
inline void Log<Level,Name,Sinks...>::log_at_level (const char* fmt, Ps...ps)
{
	if constexpr (Level >= Lvl) {
		if (level < Lvl) return;
		fmtprint(Lvl, fmt, ps...);
	}
}
