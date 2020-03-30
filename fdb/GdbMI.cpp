#include "ui_mainwindow.h"
#include "ui_settings.h"
#include "GdbMI.h"

PGdbMI GdbMI::single;

GdbMI::GdbMI(PMainWindow _ui)
  : AsyncPty("gdbmi"),
    traceeStatus(NOT_RUNNING),
    ui(_ui)
{

}


GdbMI::~GdbMI()
{
  ui.reset();
}


void GdbMI::initialize(PMainWindow ui)
{
  if (!single) {
    single = std::make_shared<GdbMI>(ui);
  }
}


void GdbMI::finalize()
{
  single.reset();
}

void GdbMI::handle_exec (QTextEvent &qte)
{
  // * "star" messages indicate changes in executable running state

  const QString& text = qte.text;
  int i=-1;
  if (i = text.indexOf("stopped", 1); i >= 0) {
    auto paramIdx = text.splitRef(',',QString::SkipEmptyParts);

  } else if (i = text.indexOf("running", 1); i >= 0) {

  }
}

void GdbMI::handle_notify (QTextEvent &qte)
{

}

void GdbMI::handle_status (QTextEvent &qte)
{

}

void GdbMI::handle_console (QTextEvent &qte)
{

}

void GdbMI::handle_target(QTextEvent &qte)
{

}

void GdbMI::handle_log(QTextEvent &qte)
{

}

void GdbMI::handleLibraryStmt(QTextEvent &qte)
{

}

void GdbMI::handleThreadStmt(QTextEvent &qte)
{

}

void GdbMI::handleStatusStmt(QTextEvent &qte)
{

}

void GdbMI::dispatchEvent(QTextEvent &event)
{
  switch (event.ttype)
  {
  case QTextEvent::Type::Input:
    input(event);
    break;
  case QTextEvent::Type::Output:
    dispatchOutput(event);
    break;
  case QTextEvent::Type::Error:
    error(event);
    break;
  case QTextEvent::Opened:
  case QTextEvent::Closed:
  case QTextEvent::Hangup:
    status(event);
    break;
  case QTextEvent::Noop:
    ;
  }
}

void GdbMI::dispatchOutput(QTextEvent &te)
{
  const QString& text = te.text;
  QPlainTextEdit* edit = nullptr;

  if (text.startsWith('*')) {
    handle_exec(te);
  } else if (text.startsWith("+")) {
    handle_status(te);
  } else if (text.startsWith('=')) {
    handle_notify(te);
  } else if (text.startsWith('~')) {
    handle_console(te);
  } else if (text.startsWith('@')) {
    handle_target(te);
  } else if (text.startsWith('&')) {
    handle_log(te);
  }
#undef HANDLE_TEXT

}
