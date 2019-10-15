#ifndef GDBPROCESS_H
#define GDBPROCESS_H

#include <QProcess>

class GDBProcess : public QProcess
{
public:
  GDBProcess();
  GDBProcess(QObject* parent);
};

#endif // GDBPROCESS_H
