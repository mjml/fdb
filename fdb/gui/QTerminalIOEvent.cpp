#include "gui/QTerminalDock.h"
#include "QTerminalIOEvent.h"

QTerminalIOEvent::QTerminalIOEvent(QString&& qs)
  : QEvent(QTerminalDock::ioEventType), text(qs), display(true)
{
}

QTerminalIOEvent::QTerminalIOEvent()
  : QEvent(QTerminalDock::ioEventType), text(), display(false)
{

}
