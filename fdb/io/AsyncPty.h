#ifndef ASYNCPTY_H
#define ASYNCPTY_H

#include <QTextCharFormat>
#include <QEvent>
#include <QObject>
#include "io/EPollDispatcher.h"
#include "util/QTextEvent.h"
#include "util/safe_deque.hpp"
#include "util/exceptions.hpp"

/**
 * Historically, QTerminalDock held all the required data and code for talking to EPollDispatcher
 * and it delivered data directly to its UI in the gui thread via events.
 *
 * Moving forward, this class will encapsulate IO for e.g. pseudoterminals used for gdb, gdbmi, and the tracee,
 * then expose i/o events to a multitude of listeners, the former QTerminalDock family widgets being one of those.
 *
 */
class AsyncPty : public QObject
{
  Q_OBJECT

public:

protected:
  QString                     _name;
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
  std::list<QString>          accept_prefixes;

public:
  explicit AsyncPty (const QString& name, QObject *parent = nullptr);
  virtual ~AsyncPty() override;

  const QString& name();
  const QString  ptyname();


  /**
   * @brief Called to set up the EPoll in the GUI thread before control is handed over to the IO thread.
   * Must be called only from the gui thread.
   */
  void createPty();

  /**
   * @brief The pty is gone when the slave closes it, so need to have a way to cleanly shut down the master.
   * Can be called from any thread. Will use postEvent() to fire the close signal in the gui thread.
   */
  void destroyPty();

  /**
   * @brief Custom event handling to receive terminal output from the IO thread to the gui thread via QCoreApplication.
   */
  virtual bool event (QEvent* e) override;

  /**
   * @brief Writes a string text input to the pty.
   * @param qs A QString of text. A linefeed will be appended if needed.
   */
  void write (QString& qs);

  /**
   * @brief Helper writer for formatted Latin1 input.
   * @param fmt
   */
  void write (const char* fmt, ...);

signals:
  /**
   * Invoked when the pseudoterminal is opened, closed, or hupped.
   */
  void status (QTextEvent& qte);

  /**
   * Invoked in the gui thread on output events.
   */
  void output (QTextEvent& qte);

  /**
   * Invoked in the gui thread on write() and writeInput() events (write or writeInput).
   */
  void input (QTextEvent& qte);

  /**
   * Error received by gui thread.
   */
  void error (QTextEvent& qte);

  /**
   * The privileged view will need to supply these, otherwise they default to 80x40.
   */
  void needsTerminalDimensions (int& width, int& height, int& xpixel, int& ypixel);

public slots:
  /**
   * Call this to inform the pseudoterminal machinery of a change in terminal size.
   * Intended to be set by a "privileged" view like the QTerminalDock.
   */
  void setTerminalDimensions (int width, int height, int xpixel, int ypixel);

protected:
  EPollListener::ret_t onEPollIn (const struct epoll_event& eev);

  EPollListener::ret_t onEPollOut (const struct epoll_event& eev);

  EPollListener::ret_t onEPollHup (const struct epoll_event& eev);

  virtual void dispatchEvent (QTextEvent& event);

};

typedef std::shared_ptr<AsyncPty> PAsyncPty;


#endif // ASYNCPTY_H
