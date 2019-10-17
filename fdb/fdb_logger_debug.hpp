#ifndef FDB_LOGGER_HPP
#define FDB_LOGGER_HPP

#include "util/log.hpp"
#include <stdio.h>

// The Stdio Sink: (this is now provided by log.hpp by default)
extern const char stdioname[];
extern template class Log<100,stdioname,FILE>;
typedef Log<100,stdioname,FILE> StdioSink;

// The File log Sink:
extern const char mainlogfilename[];
extern template class Log<100,mainlogfilename,FILE>;
typedef Log<100,mainlogfilename,FILE> MainLogFileSink;

// The main application logger, which sends its output to each of the previous sinks. No more popen("tee ...") !
extern const char applogname[];
extern template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;
typedef Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink> Logger;


#endif // FDB_LOGGER_HPP
