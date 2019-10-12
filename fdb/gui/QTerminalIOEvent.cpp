#include "gui/QTerminalDock.h"
#include "QTerminalIOEvent.h"

QTerminalIOEvent::QTerminalIOEvent(QString&& qs)
  : QEvent(QTerminalDock::terminalOutputEventType), text(qs)
{
}
