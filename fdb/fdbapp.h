#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "util/log.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include "ipc/mqueue.hpp"
#include "TraceeProcess.h"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSettings>
#include <map>
#include <util/co_work_queue.hpp>
#include "GdbMI.h"

class GdbMI;


typedef boost::interprocess::message_queue mqueue;

class FDBApp : public QMainWindow
{
  Q_OBJECT

public:
  explicit FDBApp(QWidget *parent = nullptr);
  virtual ~FDBApp() override;

  void initialize_fdbsocket ();
  void finalize_fdbsocket ();

  void initialize_actions ();

  void read_settings ();
  void write_settings ();

private:
  PMainWindow          ui;
  Ui::SettingsDialog*  settingsUi;

  GDBProcess           gdb;
  PAsyncPty            gdbpty;
  PGdbMI               gdbmi;

  TraceeProcess        tracee;
  PAsyncPty            traceepty;


  enum ProgState {
    ProgramStart,
    NotRunning,
    Initializing,
    Initialized
  } fState, gState;

  EPollListener::pfd_t fdbsocket;

  typedef co_work_queue<QTextEvent*> CoworkQueue;
  typedef CoworkQueue::coro_t::pull_type influent_t;
  typedef CoworkQueue::coro_t::push_type effluent_t;

  CoworkQueue workq;

public slots:
  void start_tracee();

  void kill_tracee();

  void restart_gdb();

  void kill_gdb();

  void attach_to_factorio ();

  void attach_gdbmi();

  void parse_gdb_lines(QTextEvent& event);

  void parse_tracee_lines(QTextEvent& event);

  void parse_gdbmi_lines(QTextEvent& event);



protected:
  void on_tracee_finished(int exitCode, QProcess::ExitStatus exitStatus);
  void on_gdb_finished(int exitCode, QProcess::ExitStatus exitStatus);
  EPollListener::ret_t on_fdbsocket_event (const epoll_event& eev);
  EPollListener::ret_t on_fdbstubsocket_event (const epoll_event& eev);

protected:
  virtual void showEvent(QShowEvent* event) override;

};

extern int gdbpid;

#endif // MAINWINDOW_H
