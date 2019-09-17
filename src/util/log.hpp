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

#ifndef APPNAME
#define APPNAME "app"
#endif

enum LogLevel {
								NONE = 0,
								CRITICAL, // An invariant is breached, or program state is jeopardized so that exit is required.
								ERROR,    // An invariant is breached, the current user operation/intent will fail, but the program can recover or resume.
								WARNING,  // An invariant may be breached, but program state is OK and the user operation will succeed.
								PRINT,    // Status messages that record nominal execution but that is neither voluminous nor proportional to input complexity.
								FUSS,     // Unexpected condition that is part of proper execution but may indicate improper usage by the user.
								INFO,     // Status messages that illustrate program state that can be voluminous.
								DETAIL,   // Status messages that can be volumous in proportion to input complexity.
								DBG,    // Messages that are intended to show specific information with the intent of detecting preconditions to failure.
								DBG2    // The firehose.
};


struct default_logger_traits
{
	constexpr static const char* name = APPNAME;
	constexpr static int logLevel = 5;
};


template <class logger_traits = default_logger_traits>
struct Log
{
	static FILE* logfile;
	
protected:
	static void ezprint (const char* fmt, ...);
	
public:

	static void initialize ();
	
	static void finalize ();
	
	static void detail (const char* fmt, ...);
	
	static void info (const char* fmt, ...);

	static void print (const char* fmt, ...);

	static void fuss (const char* fmt, ...);
	
	static void warning (const char* fmt, ...);
	
	static void error (const char* fmt, ...);
	
	static void critical (const char* fmt, ...);
	
};



template <class logger_traits>
FILE* Log<logger_traits>::logfile;


template<class logger_traits>
void Log<logger_traits>::initialize ()
{
	using namespace std;
	pid_t mypid = getpid();
	stringstream sstrfn;
	stringstream sstrdir;
	sstrdir << "/tmp/" << logger_traits::name;
	mkdir(sstrdir.str().c_str(),S_IWUSR|S_IRUSR|S_IXUSR);
	sstrfn << "/tmp/" << logger_traits::name << "/log";
	logfile = fopen(sstrfn.str().c_str(), "w+");
	if (!logfile) { throw errno_exception(std::runtime_error); }
}


template<class logger_traits>
inline void Log<logger_traits>::finalize ()
{
	assert(logfile != nullptr);
	if (!fclose(logfile)) { throw errno_exception(std::runtime_error); }
}


template<class logger_traits>
inline void Log<logger_traits>::ezprint (const char* fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	char entry[1024];
	char msg[960];
	int chrs = vsnprintf(msg,960,fmt,ap);
	va_end(ap);
	
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &ts); 
	chrs = snprintf(entry, 1024,"[%s][%ld.%ld] %s", logger_traits::name, ts.tv_sec, ts.tv_nsec, msg);
	
	if (!fwrite(entry,1,chrs,logfile)) {
		throw errno_runtime_error;
	}
	if (!fputc('\n',logfile)) {
		throw errno_runtime_error;
	}
	if (fflush(logfile)) {
		throw errno_runtime_error;
	}
}


template<class logger_traits>
inline void Log<logger_traits>::detail (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::DETAIL) return;
	ezprint(fmt);
}


template<class logger_traits>
inline void Log<logger_traits>::info (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::INFO) return;
	ezprint(fmt);	
}


template<class logger_traits>
inline void Log<logger_traits>::print (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::PRINT) return;
	ezprint(fmt);
}


template<class logger_traits>
inline void Log<logger_traits>::fuss (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::FUSS) return;
	ezprint(fmt);	
}


template<class logger_traits>
inline void Log<logger_traits>::warning (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::WARNING) return;
	ezprint(fmt);	
}


template<class logger_traits>
inline void Log<logger_traits>::error (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::ERROR) return;
	ezprint(fmt);	
}


template<class logger_traits>
inline void Log<logger_traits>::critical (const char* fmt, ...)
{
	if (logger_traits::logLevel < LogLevel::CRITICAL) return;
	ezprint(fmt);	
}



// Our core application logger, a singleton.
typedef Log<> log;
