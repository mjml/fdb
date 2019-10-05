#include "util/log.hpp"
#include "factstub_logger.hpp"


const char stdioname[] = "stdio";
const char mainlogfilename[] = "logfile";
const char applogname[] = "factstub";

template class Log<100,stdioname,FILE>;
template class Log<100,mainlogfilename,FILE>;
template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;

int myfunc ()
{
	Logger::print("myfunc works!");
	return 0;
}
