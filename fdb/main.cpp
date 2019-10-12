#include <QApplication>
#include "gui/QTerminalDock.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  QTerminalDock::registerEventTypes();
  MainWindow w;
  w.show();

  return a.exec();
}
