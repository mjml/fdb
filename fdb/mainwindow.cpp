#include <pty.h>
#include <sys/ioctl.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "util/exceptions.hpp"


MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  gdbProc(nullptr)
{
  ui->setupUi(this);

  // Join together (tabify) the debug consoles dock widgets
  QMainWindow::tabifyDockWidget(ui->stdoutDock,  ui->gdbmiDock);
  QMainWindow::tabifyDockWidget(ui->gdbmiDock,   ui->gdbDock);
  QMainWindow::tabifyDockWidget(ui->filesDock,   ui->modDock);

}


MainWindow::~MainWindow()
{
  delete ui;
}


void MainWindow::initialize_process_window(QTerminalProcess& proc, QTerminalWidget& termWidget, bool is_controlling)
{
  struct termios term; memset(&term, 0, sizeof(termios));
  term.c_iflag  |= IGNBRK | IGNPAR; // ignore break and parity checks
  term.c_cflag  |= CLOCAL | CS8;
  term.c_lflag  |= ISIG | ICANON;

  struct winsize win = termWidget.getTerminalDimensions();

  char ptyname[256] = { 0 };

  openpty(&proc.fds.fdin, &proc.fds.fdout, ptyname, &term, &win);

  if (is_controlling) {
    proc.setStandardOutputFile(ptyname);
    proc.setStandardErrorFile(ptyname);
    proc.setStandardInputFile(ptyname);
  }

}


