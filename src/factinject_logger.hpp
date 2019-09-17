#pragma once

#include "util/log.hpp"

struct factinject_logger_traits
{
	constexpr static const char* name = "factinject";
	constexpr static int logLevel = LOGLEVEL_FACTINJECT;
};

typedef Log<factinject_logger_traits> Logger;

