#include <QApplication>
#include <stdio.h>
#include <signal.h>

#include "gui/QTerminalDock.h"
#include "util/log.hpp"

#include "fdbapp.h"
#include "fdb_logger.hpp"

// The Stdio Sink: (this is now provided by log.hpp by default)
const char stdioname[] = "stdio";
template class Log<100,stdioname,FILE>;

// The File log Sink:
const char mainlogfilename[] = "logfile";
template class Log<100,mainlogfilename,FILE>;

// The main application logger, which sends its output to each of the previous sinks. No more popen("tee ...") !
const char applogname[] = "fdb";
template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;

void sigquit_handler (int sig, siginfo_t *info, void *ucontext);


int main (int argc, char *argv[])
{
  struct sigaction sa;
  memset(&sa,0,sizeof(struct sigaction));
  sa.sa_restorer = nullptr;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = sigquit_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGHUP, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);

  StdioSink::initialize_with_handle(stdout);
  MainLogFileSink::initialize_with_filename("fdb.log");
  Logger::initialize();
  EPollDispatcher::initialize();

  QApplication* a = new QApplication(argc, argv);
  QTerminalDock::registerEventTypes();

  FDBApp* w = new FDBApp;
  w->show();

  auto r = a->exec();

  delete w; w = nullptr;
  delete a; a = nullptr;

  EPollDispatcher::finalize();
  Logger::finalize();
  MainLogFileSink::finalize();
  StdioSink::finalize();

  return r;
}


#pragma GCC diagnostic ignored "-Wunused-parameter"
void sigquit_handler (int sig, siginfo_t *info, void *ucontext)
{
  fprintf(stderr, "Handling signal %d\n", sig);
  if (gdbpid) {
    if (sig == SIGQUIT || sig == SIGHUP || sig == SIGTERM || sig == SIGINT)
    kill(gdbpid,SIGTERM);
  }
}
