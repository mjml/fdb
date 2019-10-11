#ifndef QTERMINALPROCESS_H
#define QTERMINALPROCESS_H

#include <QProcess>

class QTerminalProcess : public QProcess
{
public:
  QTerminalProcess();

  // passed to openpty to create pseudoterminal for process
  struct fds_s {
    int fdin;
    int fdout;
  } fds;
};

#endif // QTERMINALPROCESS_H
