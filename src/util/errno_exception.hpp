#pragma once
#include <string.h>
#include <errno.h>
#include <stdexcept>



template<class E>
E __errno_exception (const char* file, int line)
{
	const char *reason = strerror(errno);
	char msg[1024];
	snprintf(msg,1024,"%s:%d: %s", file, line, reason);
	return E(msg);
}

#define errno_exception(E) __errno_exception<E>(__FILE__,__LINE__)

#define errno_runtime_error __errno_exception<std::runtime_error>(__FILE__,__LINE__)

