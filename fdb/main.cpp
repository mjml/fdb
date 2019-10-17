#include <QApplication>
#include <stdio.h>

#include "gui/QTerminalDock.h"
#include "util/log.hpp"

#include "mainwindow.h"

// The Stdio Sink: (this is now provided by log.hpp by default)
const char stdioname[] = "stdio";
template class Log<100,stdioname,FILE>;

// The File log Sink:
const char mainlogfilename[] = "logfile";
template class Log<100,mainlogfilename,FILE>;

// The main application logger, which sends its output to each of the previous sinks. No more popen("tee ...") !
const char applogname[] = "fdb";
template class Log<LOGLEVEL_FACTINJECT,applogname,StdioSink,MainLogFileSink>;



int main(int argc, char *argv[])
{
  StdioSink::initialize_with_handle(stdout);
  MainLogFileSink::initialize_with_filename("fdb.log");
  Logger::initialize();

  QApplication a(argc, argv);
  QTerminalDock::registerEventTypes();
  MainWindow w;
  w.show();

  auto r = a.exec();

  Logger::finalize();
  MainLogFileSink::finalize();
  StdioSink::finalize();

  return r;
}
