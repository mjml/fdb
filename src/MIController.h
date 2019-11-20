#ifndef MICONTROLLER_H
#define MICONTROLLER_H

#include <memory>
#include <QObject>
#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "gui/QMIDock.h"
#include "gui/common.h"

class MIController;
typedef std::shared_ptr<MIController>   PMIController;

/**
 * Interprets QMIDock (gdb/mi) output and updates the frontend's GUI.
 */
class MIController : public QObject
{
  Q_OBJECT
protected:
  static PMIController   defController;
  PMainWindow            ui;

public:
  explicit MIController (PMainWindow _ui);
  ~MIController();

  QMIDock* dock() { return ui->gdbmiDock; }

signals:

public slots:
    void parse_exec_line(QString& qs);

    void parse_notify_line(QString& qs);

    void parse_status_line(QString& qs);

    void parse_console_line(QString& qs);

    void parse_target_line(QString& qs);

    void parse_log_line(QString& qs);

public:
  static void initialize(PMainWindow ui);
  static void finalize();
  static PMIController def() { return defController; }

};

#endif // MICONTROLLER_H
