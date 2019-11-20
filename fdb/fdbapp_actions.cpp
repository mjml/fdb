#include "fdbapp.h"
#include "ui_mainwindow.h"


#include <QAction>

void FDBApp::initialize_actions()
{
#define CNCT(a,f) \
  connect(a, &QAction::triggered, \
          this,    &f )

  connect(ui->actionRestart_GDB, &QAction::triggered,
       this, &FDBApp::restart_gdb);

  connect(ui->actionFactorioMode, &QAction::triggered,
          this, &FDBApp::start_factorio);

  connect(ui->gdbDock, &QTerminalDock::output, this, &FDBApp::parse_gdb_lines);

  connect(ui->factorioDock, &QTerminalDock::output, this, &FDBApp::parse_factorio_lines);

  connect(ui->gdbmiDock, &QTerminalDock::output, this, &FDBApp::parse_gdbmi_lines);



  connect(&tracee, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FDBApp::on_factorio_finished);

  connect(&gdb, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FDBApp::on_gdb_finished);
}

