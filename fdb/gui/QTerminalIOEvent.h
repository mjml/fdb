#ifndef QTERMINALIOEVENT_H
#define QTERMINALIOEVENT_H

#include <QString>
#include <QEvent>

struct QTerminalIOEvent : public QEvent
{
  QTerminalIOEvent ();
  QTerminalIOEvent (QString&& qs);
  ~QTerminalIOEvent () = default;

  QString  text;
  bool     display;
  char padding[7];
};

#endif // QTERMINALIOEVENT_H
