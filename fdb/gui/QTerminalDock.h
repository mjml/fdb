#ifndef QTERMINALDOCK_H
#define QTERMINALDOCK_H

#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>

#include "gui/QOptionsDock.h"
#include "util/safe_deque.hpp"

class QTerminalDock : public QOptionsDock
{
  Q_OBJECT

public:
  enum IoView {
    OUT_ONLY,
    IN_AND_OUT,
    COMBINED
  };

public:
  QTerminalDock();
  QTerminalDock(QWidget* parent);
  ~QTerminalDock() override;

  const QString ptyname();

  struct winsize terminalDimensions ();

  void setIoView (IoView _view);
  IoView ioView () { return ioview; }

  /**
   * @brief Called by the GUI thread to start IO polling in a separate thread
   */
  void startIOThread ();

  /**
   * @brief Called by the GUI thread to stop IO polling, usually before shutdown.
   */
  void stopIOThread();

  /**
   * @brief Called to set up the EPoll in the GUI thread before control is handed over to the IO thread.
   */
  void createPty();

  /**
   * @brief The pty is gone when the slave closes it, so need to have a way to cleanly shut down the master.
   */
  void destroyPty();

  /**
   * @brief Called in a separate thread to read from the output fd and post events back to this object in the GUI thread.
   */
  void performIO ();

  /**
   * @brief The output event posted from the IO thread.
   */
  static QEvent::Type ioEventType;

  /**
   * @brief Sets up the output event posted from the IO thread.
   */
  static void registerEventTypes ();

  /**
   * @brief Custom event handling to receive terminal output from the IO thread.
   */
  virtual bool event (QEvent* e) override;

signals:
  void output (const QString& qs);
  void input (const QString& qs);

public slots:
  // titlebar controls
  void logSizeLabelClicked (bool checked);
  void logSizeEdited ();

  // input edit control
  void onTerminalOutput (const QString& qs);

  /**
   * @brief Call this when the GUI has resized the terminal window (usually the output window)
   */
  void windowSizeChanged (const struct winsize& newSize);
  void windowSizeChanged (unsigned short rows, unsigned short cols);

  /**
   * @brief Writes a string of text to this process' stdin.
   * @param qs A QString of text. Should end with a linefeed, but not necessary.
   */
  void writeInput (const QString& qs);

  /**
   * @brief This is the handler for the input line edit control
   */
  void returnPressed ();



private:
  IoView            ioview;
  QPushButton*      lszbtn;
  QLineEdit*        lszedit;
  QCheckBox*        freezeChk;
  QLineEdit*        inEdit;
  QPlainTextEdit*   outEdit;

  safe_deque<const QString>   inputQueue;

  int               _ptfd;
  int               _epoll;
  QThread*          _iothread;

};

#endif // QTERMINALDOCK_H
