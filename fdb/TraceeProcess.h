#ifndef FACTORIOPROCESS_H
#define FACTORIOPROCESS_H

#include <QProcess>

namespace GdbMode {
  enum  {
  NOT_RUNNING,
  STOPPED,
  RUNNING
  };
}


struct TraceeProcess : public QProcess
{
  int gdbMode;

  TraceeProcess();
  TraceeProcess(QObject* parent);
};

#endif // FACTORIOPROCESS_H
