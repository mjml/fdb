#pragma once

#include "util/log.hpp"
#include <stdio.h>

#ifndef LOGLEVEL_FDBSTUB
#define LOGLEVEL_FDBSTUB 10
#endif

extern const char stdioname[];
extern template struct Log<100,stdioname,FILE>;
typedef Log<100,stdioname,FILE> StdioSink;

extern const char applogname[];
extern template struct Log<LOGLEVEL_FDBSTUB,applogname,StdioSink>;
typedef Log<LOGLEVEL_FDBSTUB,applogname,StdioSink> Logger;
