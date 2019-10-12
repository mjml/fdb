#ifndef QTERMINALDOCK_H
#define QTERMINALDOCK_H

#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>

#include "gui/QOptionsDock.h"

class QTerminalDock : public QOptionsDock
{
public:
  QTerminalDock();
  QTerminalDock(QWidget* parent);
  ~QTerminalDock();

public:
  void setProcess (QProcess* process);
  QProcess* process();

  void logSizeLabelClicked (bool checked);
  void logSizeEdited ();

private:
  QPushButton* lszbtn;
  QLineEdit*   lszedit;
  QCheckBox*   freezeChk;
  QProcess* _process;
};

#endif // QTERMINALDOCK_H
