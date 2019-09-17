#pragma once

#include <stdarg.h>


template<class E>
inline void assert_e (bool condition, const char* fmt, ...)
{
	if (condition) return;
	
	char msg[4096];
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(msg,4096,fmt,ap);
	va_end(ap);

	throw E(msg);
}

inline void assert_rune (bool condition, const char* fmt, ...)
{
	assert_e<std::runtime_error> (condition, fmt);
}
