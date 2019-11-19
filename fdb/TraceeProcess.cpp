#include "TraceeProcess.h"

TraceeProcess::TraceeProcess()
  : gdbMode(GdbMode::NOT_RUNNING)
{

}

TraceeProcess::TraceeProcess(QObject *parent)
  : QProcess(parent), gdbMode(GdbMode::NOT_RUNNING)
{

}
