#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "util/log.hpp"
#include "gui/QTerminalDock.h"
#include "util/GDBProcess.h"
#include "FactorioProcess.h"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <map>
#include <boost/coroutine2/coroutine.hpp>


namespace Ui {
class MainWindow;
class SettingsDialog;
}

struct TerminalEvent
{
  enum Type {
    Initial,
    Output
  } type;

  QString qs;
  TerminalEvent() : type(Initial), qs() {}
  TerminalEvent(const QString& s) : type(Output), qs(s) {}
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  struct IOCoro {
    MainWindow* win;
    IOCoro(MainWindow* w) : win(w) {}
  };


  typedef boost::coroutines2::coroutine<TerminalEvent> coro_t;

  struct IOGiver : public IOCoro, public coro_t::push_type
  {
    template<typename Fn>
    IOGiver(MainWindow* w, Fn &&fn) : IOCoro(w), coro_t::push_type(fn) {}

    IOGiver(const IOGiver& other) = delete;
    ~IOGiver() = default;
  };

  struct IOTaker : public IOCoro, public coro_t::pull_type
  {
    template<typename Fn>
    IOTaker(MainWindow* w, Fn &&fn) : IOCoro(w), coro_t::pull_type(fn) {}

    IOTaker(const IOTaker& other) = delete;
    ~IOTaker() = default;
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
  coro_t::push_type* gdbmiGiver;


public slots:
  void start_factorio();

  void kill_factorio();

  void restart_gdb();

  void kill_gdb();

  void integrate ();

  void attach_gdbmi();

  void parse_gdb_lines(const QString& qs);

  void parse_factorio_lines(const QString& qs);

  void parse_gdbmi_lines(const QString& qs);

protected:
  virtual void showEvent(QShowEvent* event) override;

};



#endif // MAINWINDOW_H
