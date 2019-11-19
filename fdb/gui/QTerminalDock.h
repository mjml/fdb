#ifndef QTERMINALDOCK_H
#define QTERMINALDOCK_H

#include <sys/epoll.h>
#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QColor>
#include <QPlainTextEdit>

#include "gui/QOptionsDock.h"
#include "gui/QTerminalIOEvent.h"
#include "io/EPollDispatcher.h"
#include "util/safe_deque.hpp"
#include "util/exceptions.hpp"

class QTerminalDock : public QOptionsDock
{
  Q_OBJECT

public:
  enum IoView {
    OUT_ONLY,
    IN_AND_OUT,
    COMBINED
  };

  /**
   * @brief The output event posted from the IO thread.
   */
  static QEvent::Type ioEventType;

protected:
  IoView            ioview;
  QPushButton*      lszbtn;
  QLineEdit*        lszedit;
  QCheckBox*        freezeChk;
  QLineEdit*        inEdit;
  QPlainTextEdit*   outEdit;

  safe_deque<const QString>   inputQueue;

  EPollListener::pfd_t    _ptfd;
  QThread*                _iothread;

  static constexpr int outbuf_max_siz = 65536;
  int               outbuf_idx;
  int               outbuf_siz;
  char              outbuf[outbuf_max_siz];

  static constexpr int inbuf_max_siz = 65536;
  int               inbuf_idx;
  char              inbuf[inbuf_max_siz];

public:
  QTerminalDock();
  QTerminalDock(QWidget* parent);
  virtual ~QTerminalDock();

  const QString ptyname();

  struct winsize terminalDimensions ();

  void setIoView (IoView _view);
  IoView ioView () { return ioview; }

  /**
   * @brief Called to set up the EPoll in the GUI thread before control is handed over to the IO thread.
   */
  void createPty();

  /**
   * @brief The pty is gone when the slave closes it, so need to have a way to cleanly shut down the master.
   */
  void destroyPty();

  /**
   * @brief Sets up the output event posted from the IO thread.
   */
  static void registerEventTypes ();

  /**
   * @brief Custom event handling to receive terminal output from the IO thread.
   */
  virtual bool event (QEvent* e) override;

  /**
   * @brief Writes a string of text to this process' stdin.
   * @param qs A QString of text. A carriage return will be appended if needed.
   */
  template<typename T>
  void writeInput (T qs);

  /**
   * @brief Helper writer for formatted input to the terminal.
   * @param fmt
   */
  void write (const char* fmt, ...);

  std::list<QString> accept_suffixes;


signals:
  void output (QTerminalIOEvent& ioEvent);
  void input (const QString& qs);

public slots:
  // titlebar controls
  void logSizeLabelClicked (bool checked);
  void logSizeEdited ();

  /**
   * @brief Call this when the GUI has resized the terminal window (usually the output window)
   */
  void windowSizeChanged (const struct winsize& newSize);
  void windowSizeChanged (unsigned short rows, unsigned short cols);

  /**
   * @brief This is the handler for the input line edit control
   */
  void returnPressed ();

protected:
  virtual void onTerminalEvent (QTerminalIOEvent& qs);

  EPollListener::ret_t onEPollIn (const struct epoll_event& eev);

  EPollListener::ret_t onEPollOut (const struct epoll_event& eev);

  EPollListener::ret_t onEPollHup (const struct epoll_event& eev);

};

template<typename T>
void QTerminalDock::writeInput (T qs)
{
  if (!_ptfd)  return;
  if (!qs.endsWith(('\n'))) qs += '\n';

  QTextCharFormat fmt = outEdit->currentCharFormat();
  QTextCharFormat saved(fmt);
  fmt.setFontWeight(QFont::Bold);
  fmt.setForeground(QBrush(QColor::fromRgb(20,180,30)));
  outEdit->setCurrentCharFormat(fmt);
  outEdit->insertPlainText(qs);
  outEdit->setCurrentCharFormat(saved);

  auto appioDispatch = EPollDispatcher::def();
  inputQueue.safe_emplace_front(qs);
  appioDispatch->modify(*_ptfd, EPOLLOUT, 0);
}



#endif // QTERMINALDOCK_H
