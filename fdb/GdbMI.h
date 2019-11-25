#ifndef MICONTROLLER_H
#define MICONTROLLER_H

#include <memory>
#include <QObject>
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "gui/QMIDock.h"
#include "gui/common.h"
#include "io/AsyncPty.h"

class GdbMI;
typedef std::shared_ptr<GdbMI>   PGdbMI;

struct BreakInfo;
struct FrameInfo;
struct ThreadInfo;
struct ExceptInfo;

/**
 * Interprets QMIDock (gdb/mi) output and updates the frontend's GUI.
 */
class GdbMI : public AsyncPty
{
  Q_OBJECT

public:
    enum TraceeStatus {
      NOT_RUNNING,
      RUNNING,
      STOPPED
    } traceeStatus;

    enum Verbs {
      Created,
      Deleted,
      Exited,
      Started,
      Stopped,
      Selected,
      Loaded,
      Unloaded,
      Modified, // on request
      Changed   // asynchronously
    };

protected:
  static PGdbMI          single;
  PMainWindow            ui;

public:
  explicit GdbMI (PMainWindow _ui);
  ~GdbMI();

  QMIDock* dock() { return ui->gdbmiDock; }

signals:
  void execText(QTextEvent& qs);
  void notifyText(QTextEvent& qs);
  void statusText(QTextEvent& qs);
  void consoleText(QTextEvent& qs);
  void targetText(QTextEvent& qs);
  void logText(QTextEvent& qs);

  void tracee_info(TraceeStatus status);
  void thread_info();
  void library_info();
  void breakpoint_info();



public slots:

public:
  static void initialize(PMainWindow ui);
  static void finalize();
  static PGdbMI def() { return single; }

  void handle_exec(QTextEvent& qte);
  void handle_notify(QTextEvent& qte);
  void handle_status(QTextEvent& qte);
  void handle_console(QTextEvent& qte);
  void handle_target(QTextEvent& qte);
  void handle_log(QTextEvent& qte);

  void handleLibraryStmt(QTextEvent& qte);
  void handleThreadStmt(QTextEvent& qte);
  void handleStatusStmt(QTextEvent& qte);

protected:
  virtual void dispatchEvent (QTextEvent& event);
  virtual void dispatchOutput (QTextEvent& event);

};

#endif // MICONTROLLER_H
