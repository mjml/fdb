#ifndef QTERMINALIOEVENT_H
#define QTERMINALIOEVENT_H

#include <QString>
#include <QEvent>

struct QTerminalIOEvent : public QEvent
{
public:
  QTerminalIOEvent(QString&& qs);

  QString text;
};

#endif // QTERMINALIOEVENT_H
