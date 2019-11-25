#include "gui/QTerminalDock.h"
#include "QTextEvent.h"

QEvent::Type QTextEvent::ioEventType;

QTextEvent::QTextEvent(QTextEvent::Type _type, QString&& qs)
  : QEvent(ioEventType), ttype(_type), text(qs), display(true)
{
}

QTextEvent::QTextEvent(QTextEvent::Type _type)
  : QEvent(ioEventType), ttype(_type), text(), display(false)
{

}

void QTextEvent::registerEventType ()
{
  ioEventType = static_cast<QEvent::Type>(QEvent::registerEventType(-1));
}


