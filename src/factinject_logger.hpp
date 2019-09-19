#pragma once

#include "util/log.hpp"

struct factinject_logger_traits
{
	constexpr static const char* name = "factinject";
	constexpr static int logLevel = LOGLEVEL_FACTINJECT;
};

typedef Log<factinject_logger_traits> Logger;

template <>
inline void Log<factinject_logger_traits>::initialize ()
{
	using namespace std;
	pid_t mypid = getpid();
	stringstream sstrcmd;
	stringstream sstrdir;
	sstrdir << "/tmp/" << factinject_logger_traits::name;
	mkdir(sstrdir.str().c_str(),S_IWUSR|S_IRUSR|S_IXUSR);
	sstrcmd << "tee /tmp/" << factinject_logger_traits::name << "/log";
	//logfile = fopen(sstrfn.str().c_str(), "w+");
	logfile = popen(sstrcmd.str().c_str(), "w");
	if (!logfile) { throw errno_exception(std::runtime_error); }	
}

template <>
inline void Log<factinject_logger_traits>::finalize ()
{
	assert(logfile != nullptr);
	if (!pclose(logfile)) { throw errno_exception(std::runtime_error); }
}

