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

  connect(ui->actionFactorioMode, &QAction::triggered,
          this, &MainWindow::start_factorio);

  connect(ui->gdbDock, &QTerminalDock::output, this, &MainWindow::parse_gdb_lines);

  connect(ui->factorioDock, &QTerminalDock::output, this, &MainWindow::parse_factorio_lines);

  connect(ui->gdbmiDock, &QTerminalDock::output, this, &MainWindow::parse_gdbmi_lines);

  connect(&factorio, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::on_factorio_finished);

  connect(&gdb, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::on_gdb_finished);
}

