#ifndef QGDBMIDOCK_H
#define QGDBMIDOCK_H

#include <QTabWidget>
#include <QMenu>
#include <QGridLayout>
#include "gui/QTerminalDock.h"

class QMIDock;

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
  void handleEvent(QTerminalIOEvent& ioevent, QPlainTextEdit& mainEdit, QMIDock* dock, void (QMIDock::*sigfunc)(const QString&));

};

class QMIDock : public QTerminalDock
{
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
  ~QMIDock();

public slots:
  void onContextMenuEvent(QContextMenuEvent* event);

  void onTerminalEvent(QTerminalIOEvent &qs) override;

  void onCheckboxesUpdated (int state);

signals:
  void exec_output(const QString& text);
  void status_output(const QString& text);
  void notify_output(const QString& text);
  void console_output(const QString& text);
  void target_output(const QString& text);
  void log_output(const QString& text);

protected:
  void createPopupWidget();

  virtual void mousePressEvent(QMouseEvent* event) override;

};

#endif // QGDBMIDOCK_H
