#include "log.hpp"

// The Stdio Sink: (this is now provided by log.hpp by default)
const char stdioname[] = "stdio";
template struct Log<100,stdioname,FILE>;
typedef Log<100,stdioname,FILE> StdioSink;

// The main application logger, which sends its output to each of the previous sinks. No more popen("tee ...") !
const char applogname[] = "fdb";
template struct Log<LOGLEVEL,applogname,StdioSink>;
typedef Log<LOGLEVEL,applogname,StdioSink> Logger;

