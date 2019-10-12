#include <stdio.h>
#include <QApplication>
#include "gui/QTerminalDock.h"
#include "util/log.hpp"
#include "mainwindow.h"



int main(int argc, char *argv[])
{
  StdioSink::initialize_with_handle(stdout);
  QApplication a(argc, argv);
  QTerminalDock::registerEventTypes();
  MainWindow w;
  w.show();

  return a.exec();
}
