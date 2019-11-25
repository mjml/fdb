#include <QApplication>
#include <stdio.h>
#include <signal.h>

#include "gui/QTerminalDock.h"
#include "util/log.hpp"

#include "fdbapp.h"


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
  Logger::initialize();
  EPollDispatcher::initialize();

  QApplication* a = new QApplication(argc, argv);
  QTextEvent::registerEventType();

  FDBApp* w = new FDBApp;
  w->show();

  auto r = a->exec();

  delete w; w = nullptr;
  delete a; a = nullptr;

  EPollDispatcher::finalize();
  Logger::finalize();
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
