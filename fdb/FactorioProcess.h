#ifndef FACTORIOPROCESS_H
#define FACTORIOPROCESS_H

#include <QProcess>

class FactorioProcess : public QProcess
{
public:
  FactorioProcess();
  FactorioProcess(QObject* parent);
};

#endif // FACTORIOPROCESS_H
