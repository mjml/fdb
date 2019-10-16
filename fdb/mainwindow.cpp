#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "util/exceptions.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>



MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  gdb(this),
  factorio(this),
  fState(ProgramStart),
  gState(ProgramStart)
{
  ui->setupUi(this);

  ui->gdbmiDock->hide();

  initialize_actions();

  QObject::connect(&factorio, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                   [=](int exitCode, QProcess::ExitStatus exitStatus){
    ui->actionFactorioMode->setEnabled(true);
    ui->actionFactorioMode->setText("Run Factorio");
  });

}


MainWindow::~MainWindow()
{
  if (factorio.state() == QProcess::Running) {
    factorio.terminate();
    factorio.waitForFinished();
    fState = NotRunning;
  }
  if (gdb.state() == QProcess::Running) {
    gdb.terminate();
    gdb.waitForFinished();
    gState = NotRunning;
  }
  delete ui;
}


void MainWindow::start_factorio()
{
  if (factorio.state() == QProcess::Running) {
    Logger::warning("Can't start factorio, it is already running!");
    return;
  }

  ui->actionFactorioMode->setEnabled(false);
  ui->actionFactorioMode->setText("Factorio is running...");
  ui->factorioDock->createPty();
  ui->factorioDock->startIOThread();
  Logger::print("Starting Factorio with pseudoterminal on %s", ui->factorioDock->ptyname().toLatin1().data());

  factorio.setStandardOutputFile(ui->factorioDock->ptyname().toLatin1().data());
  factorio.setStandardErrorFile(ui->factorioDock->ptyname().toLatin1().data());

  // TODO: create a better way to manage engine versions / builds / deployments / etc.
  factorio.setWorkingDirectory("/home/joya");
  factorio.start("/home/joya/games/factorio/bin/x64/factorio");
  fState = Initializing;
}

void MainWindow::kill_factorio()
{
  factorio.close();
  ui->factorioDock->stopIOThread();
  ui->factorioDock->destroyPty();
}

void MainWindow::restart_gdb()
{
  if (gdb.state() == QProcess::Starting) {
    Logger::warning("Refusing to start gdb because process is just starting...");
    return;
  }

  if (gdb.state() == QProcess::Running) {
    Logger::warning("Restarting running gdb...");
    kill_gdb();
    QTimer::singleShot(2500, this, &MainWindow::restart_gdb);
    return;
  }

  ui->gdbDock->createPty();
  ui->gdbDock->startIOThread();

  Logger::print("Starting gdb with pseudoterminal on %s", ui->gdbDock->ptyname().toLatin1().data());

  gdb.setStandardInputFile(ui->gdbDock->ptyname().toLatin1());
  gdb.setStandardOutputFile(ui->gdbDock->ptyname().toLatin1());
  gdb.setStandardErrorFile(ui->gdbDock->ptyname().toLatin1());

  // gdb.setWorkingDirectory("..?..");

  gdb.start("/bin/gdb -ex \"set pagination off\"");
  gState = Initializing;

}

void MainWindow::kill_gdb()
{
  Logger::info("Terminating gdb process...");
  ui->gdbDock->stopIOThread();
  ui->gdbDock->destroyPty();
  gdb.kill();
  gdb.waitForFinished(-1);
}

void MainWindow::integrate()
{
  Logger::info("Integrating factorio with debugger logic.");
}

void MainWindow::attach_gdbmi()
{
  if (gdb.state() != QProcess::Running) {
    Logger::warning("gdb isn't running, can't attach gdb/mi terminal");
    return;
  }
  if (gState != Initialized) {
    Logger::warning("gdb hasn't fully initialized, can't attach gdb/mi terminal");
    return;
  }

  ui->gdbmiDock->createPty();
  ui->gdbmiDock->startIOThread();

  auto ba = ui->gdbmiDock->ptyname().toLatin1();
  char* name = ba.data();

  Logger::print("Creating gdb/mi terminal on %s", name);

  char cmd[1024];
  snprintf(cmd, 1023, "new-ui mi2 %s", name);
  QString qscmd = QString::fromLatin1(cmd);
  ui->gdbDock->writeInput(qscmd);
}

void MainWindow::parse_gdb_lines(const QString &qs)
{
  bool combinedInitialized = (gState == Initialized) && (fState == Initialized);
  if (gState != Initialized && qs.contains("(gdb)")) {
    gState = Initialized;
    attach_gdbmi();
  }
  if (!combinedInitialized && (gState == Initialized && fState == Initialized)) {
    // integration edge
    integrate();
  }
}

void MainWindow::parse_factorio_lines(const QString &qs)
{
  bool combinedInitialized = (gState == Initialized) && (fState == Initialized);
  if (fState != Initialized && qs.contains("Factorio initialised.")) {
    fState = Initialized;
    Logger::info("Factorio initialization is complete.");
  }
  if (!combinedInitialized && (gState == Initialized && fState == Initialized)) {
    // integration edge
    integrate();
  }

}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  if (gState == ProgramStart) {
    restart_gdb();
    gState = Initializing;
  }
}

