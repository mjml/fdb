#ifndef QGDBMIDOCK_H
#define QGDBMIDOCK_H

#include <QTabWidget>
#include <QMenu>
#include <QGridLayout>
#include "gui/QTerminalDock.h"

class QMIDock;

/**
 * Represents one of GDB/MI's six asynchronous channels.
 * Provides the required UI setup wiring and interaction for the dock's tabs and popup menus.
 */
struct OutputChannel
{
  OutputChannel()
    : edit(nullptr), routeChk(nullptr), timestampChk(nullptr), fmt() {}
  ~OutputChannel();
  QPlainTextEdit  *edit;
  QCheckBox       *routeChk;
  QCheckBox       *timestampChk;
  QTextCharFormat fmt;

  void layoutTab(QTabWidget& tab, QString text);
  void layoutPopupMenu(QWidget& ctxmenu, QGridLayout& layout, QString name, QString tooltipText, int& counter);
  void connectHandler(QMIDock* dock);
  void handleEvent(QTextEvent& ioevent, QPlainTextEdit& mainEdit, QMIDock* dock, void (QMIDock::*sigfunc)(const QString&));

};


/**
 * Graphical dock for speaking to gdb/mi.
 *
 * Historically this object was a one-stop shop class for working with the gdb/mi pseudoterminal.
 *
 * Going forward, the pty guts and signals are going to move into GDBMI / PtyIO, while this object
 * serves as a dock-based view/controller.
 */
class QMIDock : public QTerminalDock
{
  Q_OBJECT

  QTabWidget*       tab;

  OutputChannel   exec;
  OutputChannel   status;
  OutputChannel   notify;
  OutputChannel   console;
  OutputChannel   target;
  OutputChannel   log;

  QWidget          visibilityPopup;

  QImage           idleIndicator;
  QImage           stoppedIndicator;
  QImage           runIndicator;

public:
  QMIDock();
  QMIDock(QWidget* parent);
  ~QMIDock() override;

public slots:
  void onContextMenuEvent(QContextMenuEvent* event);

  void onCheckboxesUpdated (int state);

  void execText(const QTextEvent& text);
  void statusText(const QTextEvent& text);
  void notifyText(const QTextEvent& text);
  void consoleText(const QTextEvent& text);
  void targetText(const QTextEvent& text);
  void logText(const QTextEvent& text);

signals:


protected:
  void createPopupWidget();

  virtual void mousePressEvent(QMouseEvent* event) override;

};

#endif // QGDBMIDOCK_H
