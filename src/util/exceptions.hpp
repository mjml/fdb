#pragma once

#include <stdarg.h>

#ifndef DEBUG
#define DEBUG false
#endif


template<class E>
inline void assert_e (bool condition, const char* fmt, ...)
{
	if (!DEBUG || condition) return;
	
	char msg[4096];
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(msg,4096,fmt,ap);
	va_end(ap);

	throw E(msg);
}

inline void assert_re (bool condition, const char* fmt, ...)
{
	if (!DEBUG || condition) return;
	
	char msg[4096];
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(msg,4096,fmt,ap);
	va_end(ap);

	throw std::runtime_error(msg);
}
