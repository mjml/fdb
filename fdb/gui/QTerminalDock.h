#ifndef QTERMINALDOCK_H
#define QTERMINALDOCK_H

#include <sys/epoll.h>
#include <QProcess>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QColor>
#include <QPlainTextEdit>

#include "gui/QOptionsDock.h"
#include "util/QTextEvent.h"
#include "io/EPollDispatcher.h"
#include "util/safe_deque.hpp"
#include "util/exceptions.hpp"

class QTerminalDock : public QOptionsDock
{
  Q_OBJECT

public:
  enum IoView {
    OUT_ONLY,
    IN_AND_OUT,
    COMBINED
  };

protected:
  IoView            ioview;
  QPushButton*      lszbtn;
  QLineEdit*        lszedit;
  QCheckBox*        freezeChk;
  QLineEdit*        inEdit;
  QPlainTextEdit*   outEdit;

public:
  QTerminalDock();
  QTerminalDock(QWidget* parent);
  virtual ~QTerminalDock();

  void setIoView (IoView _view);
  IoView ioView () { return ioview; }

signals:
  void terminal_resize(int cols, int rows, int xpixel, int ypixel);
  void input (QTextEvent& qte);

public slots:
  void output (QTextEvent& event);
  void status (QTextEvent& event);
  void error (QTextEvent& event);

  // titlebar controls
  void logSizeLabelClicked (bool checked);
  void logSizeEdited ();

  /**
   * @brief This is the handler for the input line edit control
   */
  void returnPressed ();

  /**
   * Matches the signal interface for AsyncPty: provides terminal dimensions on its startup.
   */
  void provideTerminalDimensions(int& width, int& height, int& xpixel, int& ypixel);

protected:
  virtual void resizeEvent (QResizeEvent* event);

};


#endif // QTERMINALDOCK_H
