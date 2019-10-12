#include "log.hpp"

const char stdoutname[] = "stdout";
template struct Log<12,stdoutname,FILE>;

const char applogname[] = "fdb";
template struct Log<12,applogname,StdioSink>;
