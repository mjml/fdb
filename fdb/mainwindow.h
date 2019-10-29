#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "util/log.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include "FactorioProcess.h"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <map>
#include <util/co_work_queue.hpp>

namespace Ui {
class MainWindow;
class SettingsDialog;
}

/*
struct TerminalEvent
{
  enum Type {
    Initial,
    Output
  } type;

  QString qs;
  bool display;
  TerminalEvent() : type(Initial), qs(), display(false) {}
  TerminalEvent(const QString& s) : type(Output), qs(s), display(true) {}
};
*/

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  struct IOCoro {
    MainWindow* win;
    IOCoro(MainWindow* w) : win(w) {}
  };


public:
  explicit MainWindow(QWidget *parent = nullptr);
  virtual ~MainWindow() override;

  void initialize_actions ();

private:
  Ui::MainWindow*      ui;
  Ui::SettingsDialog*  settings;

  GDBProcess gdb;
  FactorioProcess factorio;

  enum ProgState {
    ProgramStart,
    NotRunning,
    Initializing,
    Initialized
  } fState, gState;

  //coro_t::push_type* gdbmiGiver;
  typedef co_work_queue<QTerminalIOEvent*> CoworkQueue;
  typedef CoworkQueue::coro_t::pull_type influent_t;
  typedef CoworkQueue::coro_t::push_type effluent_t;

  CoworkQueue workq;

public slots:
  void start_factorio();

  void kill_factorio();

  void restart_gdb();

  void kill_gdb();

  void attach_to_factorio ();

  void attach_gdbmi();

  void parse_gdb_lines(QTerminalIOEvent& event);

  void parse_factorio_lines(QTerminalIOEvent& event);

  void parse_gdbmi_lines(QTerminalIOEvent& event);

protected slots:
  void on_factorio_finished(int exitCode, QProcess::ExitStatus exitStatus);
  void on_gdb_finished(int exitCode, QProcess::ExitStatus exitStatus);


protected:
  virtual void showEvent(QShowEvent* event) override;

};



#endif // MAINWINDOW_H
