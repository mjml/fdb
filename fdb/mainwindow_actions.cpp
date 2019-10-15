#include "mainwindow.h"
#include "ui_mainwindow.h"


#include <QAction>

void MainWindow::initialize_actions()
{
#define CNCT(a,f) \
  connect(a, &QAction::triggered, \
          this,    &f )

  connect(ui->actionRestart_GDB, &QAction::triggered,
       this, &MainWindow::restart_gdb);
}
