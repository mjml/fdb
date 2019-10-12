#include <pty.h>
#include <sys/ioctl.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "util/exceptions.hpp"
#include "gui/QTerminalDock.h"



MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  gdbDock(nullptr),
  gmiDock(nullptr),
  factorioDock(nullptr),
  gdbProc(nullptr)
{
  ui->setupUi(this);

  factorioDock = new QTerminalDock(this);
  factorioDock->setWindowTitle("factorio");
  factorioDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  factorioDock->setIoView(QTerminalDock::IoView::OUT_ONLY);
  factorioDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::BottomDockWidgetArea, factorioDock);

  gdbDock = new QTerminalDock(this);
  gdbDock->setWindowTitle("gdb");
  gdbDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  gdbDock->setIoView(QTerminalDock::IoView::IN_AND_OUT);
  gdbDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::BottomDockWidgetArea, gdbDock);

  gmiDock = new QTerminalDock(this);
  gmiDock->setWindowTitle("gdb/mi");
  gmiDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  gmiDock->setIoView(QTerminalDock::IoView::OUT_ONLY);
  gmiDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::BottomDockWidgetArea, gmiDock);
  gmiDock->hide();

  // Join together (tabify) certain dock widgets
  QMainWindow::tabifyDockWidget(ui->filesDock,   ui->modDock);
  QMainWindow::tabifyDockWidget(factorioDock,  gdbDock);
  //QMainWindow::tabifyDockWidget(gdbDock,   gmiDock);

  initialize_actions();

  initialize_gdb();
}


MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::initialize_actions()
{
#define CNCT(a,f) \
  connect(a, &QAction::triggered, \
          this,    &f )

  connect(ui->actionShow_GDB_terminal, &QAction::triggered,
          this, [&]() { gdbDock->show(); addDockWidget(Qt::BottomDockWidgetArea, gdbDock); });

  connect(ui->actionRestart_GDB, &QAction::triggered,
          this, &MainWindow::start_gdb);
}

void MainWindow::initialize_factorio()
{

}

void MainWindow::start_factorio()
{

}

void MainWindow::kill_factorio()
{

}

void MainWindow::initialize_gdb()
{
  gdbProc = new QProcess(this);

  struct termios term; memset(&term, 0, sizeof(termios));
  term.c_iflag  |= IGNBRK | IGNPAR; // ignore break and parity checks
  term.c_cflag  |= CLOCAL | CS8;
  term.c_lflag  |= ISIG | ICANON;

  struct winsize win = gdbDock->terminalDimensions();

  char ptyname1[256] = { 0 };
  char ptyname2[256] = { 0 };

  // First pty gets passed into QProcess as stdout/stdin/stderr
  openpty(&gdbDock->fds().fdin, &gdbDock->fds().fdout, ptyname1, &term, &win);
  gdbProc->setStandardOutputFile(ptyname1);
  gdbProc->setStandardErrorFile(ptyname1);
  gdbProc->setStandardInputFile(ptyname1);
  gdbDock->setupEPoll();

  // Second one gets hooked into gmiDock, and the name saved to pass into gdb to enable gdb/mi.
  openpty(&gmiDock->fds().fdin, &gdbDock->fds().fdout, ptyname2, &term, &win);
  gmiDock->setupEPoll();

}

void MainWindow::start_gdb()
{
  assert_re(gdbProc, "No gdbProc set!");
  Logger::print("Starting gdb...");

  gdbDock->startIOThread();
  gdbProc->start(QStringLiteral("/bin/gdb"));

}

void MainWindow::restart_gdb()
{
  gdbProc->waitForFinished(1000);
  while (gdbProc->state() != QProcess::ProcessState::NotRunning) {
    gdbProc->terminate();
    gdbProc->waitForFinished(500);
  }
  start_gdb();
}

void MainWindow::kill_gdb()
{

}




