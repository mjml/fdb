#include <QBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include "QMIDock.h"

QMIDock::QMIDock()
  : QTerminalDock(),
    idleIndicator(QLatin1String(":/resources/icons/red_light.png")),
    stoppedIndicator(QLatin1String(":/resources/icons/amber_light.png")),
    runIndicator(QLatin1String(":/resources/icons/green_light.png"))
{

  tab = new QTabWidget();
  QFont trmfont("Monospace", 7);
  tab->setFont(trmfont);

#define DEFINE_TAB(type) \
  type.edit = new QPlainTextEdit(); \
  type.edit->setFont(trmfont); \
  type.edit->setReadOnly(true); \
  type.tabPosition = tab->addTab(type.edit, QLatin1String(#type))

  //DEFINE_TAB(exec);
  //DEFINE_TAB(status);
  //DEFINE_TAB(notify);
  //DEFINE_TAB(console);
  //DEFINE_TAB(target);
  //DEFINE_TAB(log);
#undef DEFINE_TAB

  exec.fmt.setForeground(QBrush(Qt::darkRed));
  status.fmt.setForeground(QBrush(Qt::darkBlue));
  notify.fmt.setForeground(QBrush(QColor::fromRgb(0,40,60))); // basically darker cyan
  console.fmt.setForeground(QBrush(QColor::fromRgb(80,60,10))); // basically orange
  target.fmt.setForeground(QBrush(Qt::darkGray));
  log.fmt.setForeground(QBrush(Qt::darkMagenta));

  //createPopupWidget();
  //tab->setContextMenuPolicy(Qt::CustomContextMenu);
  //connect(tab, &QWidget::customContextMenuRequested, this, [&](const QPoint& pos){ visibilityPopup.move(pos.x()-5, pos.y()-5); visibilityPopup.show(); });

  //onCheckboxesUpdated(-1);

  /*
  QWidget* clientWidget = this->widget();
  auto oldlyt = clientWidget->layout();
  oldlyt->removeWidget(outEdit);
  oldlyt->removeWidget(inEdit);

  tab->insertTab(0,outEdit,"main");

  QWidget* newClientWidget = new QWidget();
  auto lyt = new QVBoxLayout();
  lyt->addWidget(tab);
  lyt->addWidget(inEdit);
  newClientWidget->setLayout(lyt);

  this->setWidget(newClientWidget);

  delete clientWidget;
*/
}


void QMIDock::createPopupWidget()
{
  QWidget& widget = visibilityPopup;
  auto layout = new QGridLayout();
  layout->setColumnMinimumWidth(0, 96);
  int i = 0;
  QLabel* lbl = nullptr;

  QFont mini("Monospace", 6);
  auto hdr = new QLabel("vis");
  auto tslbl = new QLabel("tstamp");
  hdr->setFont(mini);
  tslbl->setFont(mini);
  layout->addWidget(hdr,0,0,1,1);
  layout->addWidget(tslbl,0,0,1,2);

#define DEFINE_ENTRY(x,tt,cnt)  \
  x.routeChk = new QCheckBox(#x); \
  x.routeChk->setToolTip(QLatin1String(tt "\nSets " #x " output to either:\ninvisible (unchecked)\nmerged (solid)\nseparate (checked)")); \
  x.routeChk->setTristate(true); \
  x.timestampChk = new QCheckBox(); \
  lbl = new QLabel(#x); \
  lbl->setFont(QFont("Monospace", 6)); \
  layout->addWidget(lbl, cnt, 0); \
  layout->addWidget(x.routeChk, cnt, 1);\
  layout->addWidget(x.timestampChk, cnt++, 2)

  DEFINE_ENTRY(exec,"exec output shows changes in the running state of the program",i);
  exec.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_ENTRY(status,"status output shows the ongoing progress of long operations",i);
  status.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_ENTRY(notify,"notify output tracks the creation of gdb objects such as\nbreakpoints, watchpoints, threads, and so on",i);
  notify.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_ENTRY(console,"console output is human-readable output in response to a CLI command",i);
  console.routeChk->setCheckState(Qt::Checked);
  DEFINE_ENTRY(target,"target output is produced by the tracee program itself",i);
  target.routeChk->setCheckState(Qt::Checked);
  DEFINE_ENTRY(log,"log output comes from gdb's internals such as errors and warnings",i);
  log.routeChk->setCheckState(Qt::Checked);
#undef DEFINE_ENTRY

#define CONNECT_ROUTE(x) \
  connect(x.routeChk, &QCheckBox::stateChanged, \
          this, &QMIDock::onCheckboxesUpdated)

  //CONNECT_ROUTE(exec);
  //CONNECT_ROUTE(status);
  //CONNECT_ROUTE(notify);
  //CONNECT_ROUTE(console);
  //CONNECT_ROUTE(target);
  //CONNECT_ROUTE(log);


  widget.setLayout(layout);

}

QMIDock::QMIDock(QWidget* parent)
  : QMIDock()
{
  this->setParent(parent);
}

QMIDock::~QMIDock()
{
  tab->clear();

#define DEL_ENTRY(x) \
  //delete x.edit

  DEL_ENTRY(exec);
  DEL_ENTRY(status);
  DEL_ENTRY(notify);
  DEL_ENTRY(console);
  DEL_ENTRY(target);
  DEL_ENTRY(log);
}

void QMIDock::onContextMenuEvent(QContextMenuEvent *event)
{
  visibilityPopup.move(event->x()-2, event->y()-2);
  visibilityPopup.show();
}

void QMIDock::onTerminalEvent(QTerminalIOEvent &event)
{
  const QString& text = event.text;
  QPlainTextEdit* edit = nullptr;
#define HANDLE_TEXT(type) \
  if (type.routeChk->checkState() == Qt::Checked) { \
    edit = type.edit; \
  } else if (type.routeChk->checkState() == Qt::PartiallyChecked) { \
    edit = outEdit; \
  } \
  if (edit) { \
    QTextCharFormat saved(edit->currentCharFormat()); \
    edit->setCurrentCharFormat(type.fmt); \
    if (type.timestampChk->checkState() == Qt::Checked) { \
       struct timespec ts; \
       char timestamp[1024]; \
       clock_gettime(CLOCK_MONOTONIC_COARSE, &ts); \
	   snprintf(timestamp, 1023, "%ld.%06ld ", ts.tv_sec, ts.tv_nsec/1000);\
       edit->insertPlainText(QLatin1String(timestamp)); \
    } \
    edit->insertPlainText(event.text); \
    edit->setCurrentCharFormat(saved); \
  } \
  type##_output(event.text)

  if (text.startsWith('*')) {
    //HANDLE_TEXT(exec);
  } else if (text.startsWith("+")) {
    //HANDLE_TEXT(status);
  } else if (text.startsWith('=')) {
    //HANDLE_TEXT(notify);
  } else if (text.startsWith('~')) {
    //HANDLE_TEXT(console);
  } else if (text.startsWith('@')) {
    //HANDLE_TEXT(target);
  } else if (text.startsWith('&')) {
    //HANDLE_TEXT(log);
  } else {
    QTerminalDock::onTerminalEvent(event);
  }

}

void QMIDock::exec_output(const QString &text)
{

}

void QMIDock::status_output(const QString &text)
{

}

void QMIDock::notify_output(const QString &text)
{

}

void QMIDock::console_output(const QString &text)
{

}

void QMIDock::target_output(const QString &text)
{

}

void QMIDock::log_output(const QString &text)
{

}



void QMIDock::onCheckboxesUpdated (int state)
{
  tab->clear();

  tab->addTab(outEdit, "main");

#define CHKTAB(x) \
  if (x.routeChk->checkState() == Qt::Checked) { \
    tab->addTab(x.edit, #x); \
  } \
  x.edit->setVisible(true)

  CHKTAB(exec);
  CHKTAB(status);
  CHKTAB(notify);
  CHKTAB(console);
  CHKTAB(target);
  CHKTAB(log);

}

