#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "util/exceptions.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include <QStringLiteral>
#include <QRegExp>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>



MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  gdb(this),
  factorio(this),
  fState(ProgramStart),
  gState(ProgramStart),
  workq()
{
  ui->setupUi(this);

  //ui->gdbmiDock->hide();

  initialize_actions();

#pragma GCC diagnostic ignored "-Wunused-parameter"
  QObject::connect(&factorio, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                   [&](int exitCode, QProcess::ExitStatus exitStatus){
    ui->actionFactorioMode->setEnabled(true);
    ui->actionFactorioMode->setText("Run Factorio");
  });
#pragma GCC diagnostic warning "-Wunused-parameter"

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

  //if (gdbmiGiver) delete gdbmiGiver;
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

void MainWindow::attach_to_factorio()
{
  effluent_t worker([&] (influent_t& src) {
    ui->gdbmiDock->write("-target-attach %d", factorio.pid());
    do { src(); } while (!src.get()->text.contains("^done"));
    ui->gdbmiDock->write("100-data-evaluate-expression \"(long)dlopen(\\\x22/home/joya/localdev/factinject/fdbstub/builds/debug/libfdbstub.so\\\x22, 2)\"");
    do { src(); } while (!src.get()-> text.startsWith("100^done"));
    src.get()->text.toLatin1().data();
    QRegExp retvalue_regex(QLatin1String("value=\"(\\d+)\""));
    int pos = retvalue_regex.indexIn(src.get()->text, 8);
    if (pos != -1) {
      // address of stub at:
      QString cap = retvalue_regex.cap(1);
      bool ok = false;
      unsigned long long stub_address = cap.toULongLong(&ok,10);
      Logger::print("Injected stub at 0x%llx", stub_address);
    } else {
      // Couldn't find address of stub
      Logger::print("Couldn't inject stub executable.");
    }

    ui->gdbmiDock->write("-exec-continue --all");
    do { src(); } while (!src.get()->text.startsWith("^running"));
  });
  QTerminalIOEvent initial;
  worker(&initial);
  workq.push_worker(std::move(worker));
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

  effluent_t worker([&] (influent_t& src){
    ui->gdbDock->write("new-ui mi2 %s", name);
    src();
    QTerminalIOEvent* ptEvent = src.get();
    //Logger::debug("[gdbmi-ack]: %s", ptEvent->text.toLatin1().data());
  });
  QTerminalIOEvent initial;
  worker(&initial);
  workq.push_worker(std::move(worker));

}

void MainWindow::parse_gdb_lines(QTerminalIOEvent& event)
{
  bool combinedInitialized = (gState == Initialized) && (fState == Initialized);
  if (gState != Initialized && event.text.contains("(gdb)")) {
    gState = Initialized;
    attach_gdbmi();
  }
  if (!combinedInitialized && (gState == Initialized && fState == Initialized)) {
    // integration edge
    attach_to_factorio();
  }
}

void MainWindow::parse_factorio_lines(QTerminalIOEvent& event)
{
  bool combinedInitialized = (gState == Initialized) && (fState == Initialized);
  if (fState != Initialized && event.text.contains("Factorio initialised")) {
    fState = Initialized;
    Logger::info("Factorio initialization is complete.");
  }
  if (!combinedInitialized && (gState == Initialized && fState == Initialized)) {
    // integration edge
    attach_to_factorio();
  }

}

void MainWindow::parse_gdbmi_lines(QTerminalIOEvent& event)
{
  //char* pc = qs.toLatin1().data();
  workq.push_input(&event);
}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  if (gState == ProgramStart) {
    restart_gdb();
    gState = Initializing;
  }
}

