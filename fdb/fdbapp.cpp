#include "fdbapp.h"
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "util/exceptions.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <QStringLiteral>
#include <QRegExp>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>

FDBApp::FDBApp(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  gdb(this),
  factorio(this),
  fState(ProgramStart),
  gState(ProgramStart),
  fdbsock(0),
  workq()
{
  ui->setupUi(this);

  read_settings();

  initialize_actions();

  fdbsock = ::socket(AF_UNIX, SOCK_STREAM, 0);

  const char* fdbsock_name = "fdbsocket";
  sockaddr_un sa;
  memset(&sa, 0, sizeof(sockaddr_un));
  sa.sun_family = AF_UNIX;
  strcpy(sa.sun_path+1, fdbsock_name);

  int r = ::bind(fdbsock,
                 reinterpret_cast<const struct sockaddr*>(&sa),
                 static_cast<socklen_t>(sizeof(sa_family_t) + strlen(fdbsock_name) + 1));
  assert_re(r == 0, "Couldn't find the fdbsocket UNIX socket. strerror(errno) = \"%s\"", strerror(errno));

}


FDBApp::~FDBApp()
{
  write_settings();

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
  ::close(fdbsock);
}

void FDBApp::read_settings ()
{
  QSettings settings("Hermitude", "fdb");

  settings.beginGroup("MainWindow");
  auto size = settings.value("size");
  if (!size.isNull())  {
    resize(size.toSize());
  }
  auto pos = settings.value("pos");
  if (!pos.isNull()) {
    move(pos.toPoint());
  }
  settings.endGroup();
}

void FDBApp::write_settings ()
{
  QSettings settings("Hermitude", "fdb");

  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
}

void FDBApp::start_factorio()
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

void FDBApp::kill_factorio()
{
  factorio.close();
  ui->factorioDock->stopIOThread();
  ui->factorioDock->destroyPty();
}

void FDBApp::restart_gdb()
{
  if (gdb.state() == QProcess::Starting) {
    Logger::warning("Refusing to start gdb because process is just starting...");
    return;
  }

  if (gdb.state() == QProcess::Running) {
    Logger::warning("Restarting running gdb...");
    kill_gdb();
    QTimer::singleShot(2500, this, &FDBApp::restart_gdb);
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

void FDBApp::kill_gdb()
{
  Logger::info("Terminating gdb process...");
  ui->gdbDock->stopIOThread();
  ui->gdbDock->destroyPty();
  gdb.kill();
  gdb.waitForFinished(-1);
}

void FDBApp::attach_to_factorio()
{
  effluent_t worker([&] (influent_t& src) {
    ui->gdbmiDock->write("-target-attach %d", factorio.pid());
    do { src(); } while (!src.get()->text.contains("^done"));
    ui->gdbmiDock->write("100-data-evaluate-expression \"(long)dlopen(\\\x22/home/joya/localdev/factinject/fdbstub/builds/debug/libfdbstub.so\\\x22, 2)\"");
    do { src(); } while (!src.get()-> text.startsWith("100^done"));
    QRegExp retvalue_regex(QLatin1String("value=\"(\\d+)\""));
    int pos = retvalue_regex.indexIn(src.get()->text, 8);
    if (pos != -1) {
      // address of stub at:
      QString cap = retvalue_regex.cap(1);
      bool ok = false;
      unsigned long long dlhandle = cap.toULongLong(&ok,10);
      Logger::print("Injected stub at 0x%llx", dlhandle);
    } else {
      // Couldn't find address of stub
      Logger::print("Couldn't inject stub executable.");
    }

    ui->gdbmiDock->write("-file-symbol-file /home/joya/localdev/factinject/fdbstub/builds/debug/libfdbstub.so.1.0.0");
    do { src(); } while (!src.get()->text.contains("^done"));

    ui->gdbmiDock->write("-file-symbol-file /home/joya/games/factorio/bin/x64/factorio");
    do { src(); } while (!src.get()->text.contains("^done"));

    ui->gdbmiDock->write("101-data-evaluate-expression stub_init()");
    do { src(); } while (!src.get()->text.startsWith("101^done"));

    ui->gdbmiDock->write("-break-insert --function lua_newstate");
    do { src(); } while (!src.get()->text.startsWith("^done"));

    ui->gdbmiDock->write("-exec-continue --all");
    do { src(); } while (!src.get()->text.startsWith("^running"));

    do { src(); } while (!src.get()->text.contains("breakpoint-hit"));
    ui->gdbmiDock->write("-data-evaluate-expression $rdi");

    do { src(); } while (!src.get()->text.contains("^done,value="));
    QRegExp luaState_regexp(QLatin1String("value=\"(\\d+)\""));
    pos = luaState_regexp.indexIn(src.get()->text);
    unsigned long long pState = 0;
    if (pos != -1) {
      QString cap = luaState_regexp.cap(1);
      bool ok = false;
      pState = cap.toULongLong(&ok, 10);
      Logger::print("lua_State found at 0x%llx", pState);
    } else {
      Logger::warning("Couldn't obtain lua_State pointer.");
    }

    // So here we've got it, but we can't execute on it right away --
    // We need to pass it to some stub_register function later so that it can register its C functions.
    // Also, we may want to hook more since they are clearly doing some work with mods early on in initialization.

    // TODO: tidy up this section and review the I/O chain. The work queue could use further enhancements!

    ui->gdbmiDock->write("-break-delete 1");
    do { src(); } while (!src.get()->text.startsWith("^done"));

    // maybe set another breakpoint here for very much later, or perhaps upon returning to some function higher up the call stack

    //ui->gdbmiDock->write("102-data-evaluate-expression register_lua_state(%d)", pState);

    ui->gdbmiDock->write("-exec-continue --all");
    do { src(); } while (!src.get()->text.startsWith("^running"));

  });
  QTerminalIOEvent initial;
  worker(&initial);
  workq.push_worker(std::move(worker));
}

void FDBApp::attach_gdbmi()
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
  });
  QTerminalIOEvent initial;
  worker(&initial);
  workq.push_worker(std::move(worker));

}

void FDBApp::parse_gdb_lines(QTerminalIOEvent& event)
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

void FDBApp::parse_factorio_lines(QTerminalIOEvent& event)
{
  bool combinedInitialized = (gState == Initialized) && (fState == Initialized);
  if (fState != Initialized && event.text.contains("Factorio")) {
    fState = Initialized;
    Logger::info("Factorio initialization is complete.");
  }
  if (!combinedInitialized && (gState == Initialized && fState == Initialized)) {
    // integration edge
    attach_to_factorio();
  }

}

void FDBApp::parse_gdbmi_lines(QTerminalIOEvent& event)
{
  //char* pc = qs.toLatin1().data();
  workq.push_input(&event);
}

void FDBApp::on_factorio_finished(int exitCode, QProcess::ExitStatus exitStatus)
{
  fState = NotRunning;
  ui->actionFactorioMode->setEnabled(true);
  ui->actionFactorioMode->setText("Run Factorio");
}

void FDBApp::on_gdb_finished(int exitCode, QProcess::ExitStatus exitStatus)
{
  gState = NotRunning;
}

void FDBApp::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  if (gState == ProgramStart) {
    restart_gdb();
  }
}

