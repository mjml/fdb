#ifndef QTERMINALDOCK_H
#define QTERMINALDOCK_H

#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>

#include "gui/QOptionsDock.h"

class QTerminalDock : public QOptionsDock
{
  Q_OBJECT

public:
  enum IoView {
    OUT_ONLY,
    IN_AND_OUT,
    COMBINED
  };

  struct fds_s {
    int fdin;
    int fdout;
  };

public:
  QTerminalDock();
  QTerminalDock(QWidget* parent);
  virtual ~QTerminalDock();

public:
  struct winsize terminalDimensions ();

  void setProcess (QProcess* process);
  QProcess* process();

  void setIoView (IoView _view);
  IoView ioView () { return ioview; }
  struct fds_s& fds () { return _fds; }

  /**
   * @brief The output event posted from the IO thread.
   */
  static QEvent::Type terminalOutputEventType;

  /**
   * @brief Sets up the output event posted from the IO thread.
   */
  static void registerEventTypes ();

  /**
   * @brief Custom event handling to receive terminal output from the IO thread.
   */
  virtual bool event (QEvent* e) override;

  /**
   * @brief Called to set up the EPoll in the GUI thread before control is handed over to the IO thread.
   */
  void setupEPoll();

  /**
   * @brief Called by the GUI thread to start IO polling in a separate thread
   */
  void startIOThread ();

  void stopIOThread();

  /**
   * @brief Called in a separate thread to read from the output fd and post events back to this object in the GUI thread.
   */
  void performIO ();

signals:
  void output (const QString& qs);

public slots:
  // titlebar controls
  void logSizeLabelClicked (bool checked);
  void logSizeEdited ();

  // input edit control
  void onTerminalOutput (const QString& qs);

  // the QProcess itself
  void onErrorOccurred (QProcess::ProcessError error);
  void onFinished (int exitcode, QProcess::ExitStatus status);
  void onStateChanged (QProcess::ProcessState newState);

private:
  IoView       ioview;
  QPushButton* lszbtn;
  QLineEdit*   lszedit;
  QCheckBox*   freezeChk;

  QPlainTextEdit* outEdit;
  QLineEdit* inEdit;

  QProcess*    _process;
  QThread*     _iothread;

  fds_s        _fds;
  int          _epoll;
};

#endif // QTERMINALDOCK_H
