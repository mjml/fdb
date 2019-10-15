#pragma once

#include <stdarg.h>

#ifndef DEBUG
#define DEBUG false
#endif

#include "errno_exception.hpp"

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

template<class E>
[[noreturn]]
inline void __Throw (const char* fmt, ...)
{
	char msg[4096];
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(msg,4096,fmt,ap);
	va_end(ap);

	throw std::runtime_error(msg);
}

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)
//#define Throw(E,fmt...) __Throw< E >( __FILE__ ":" S__LINE__ " " fmt)
#define Throw(E,fmt...) __Throw< E >(fmt)


