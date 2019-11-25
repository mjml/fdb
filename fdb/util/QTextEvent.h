#ifndef QTERMINALIOEVENT_H
#define QTERMINALIOEVENT_H

#include <QString>
#include <QEvent>

struct QTextEvent : public QEvent
{
  /**
   * @brief The output event posted from the IO thread.
   */
  static QEvent::Type ioEventType;

  enum Type {
    Noop,    // used to prime coroutines
    Opened,  // sent on terminal creation
    Closed,  // sent on close (master pty closed)
    Hangup,  // terminal hangup (pts closed)
    Input,   // normal input line
    Output,  // normal output line
    Error,   // indicates error at source
  } ttype;

  QTextEvent (QTextEvent::Type _type);
  QTextEvent (QTextEvent::Type _type, QString&& qs);
  ~QTextEvent () = default;

  QString  text;
  bool     display;
  char padding[7];


  /**
   * @brief Sets up the output event posted from the IO thread.
   */
  static void registerEventType ();

};

#endif // QTERMINALIOEVENT_H
