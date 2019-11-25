#include <QBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QCheckBox>
#include "QMIDock.h"

OutputChannel::~OutputChannel()
{
  if (edit) {
    delete edit;
    edit = nullptr;
  }
  // the checkboxes are owned by the context menu widget
}

void OutputChannel::layoutTab(QTabWidget& tab, QString text) {
  edit = new QPlainTextEdit();
  edit->setFont(QFont("Monospace",7));
  edit->setReadOnly(true);
  tab.addTab(edit, text);
}


QMIDock::QMIDock()
  : QTerminalDock(),
    idleIndicator(QLatin1String(":/resources/icons/red_light.png")),
    stoppedIndicator(QLatin1String(":/resources/icons/amber_light.png")),
    runIndicator(QLatin1String(":/resources/icons/green_light.png"))
{

  tab = new QTabWidget();
  QFont trmfont("Monospace", 7);
  tab->setFont(trmfont);

#define DEFINE_TAB(channel) \
  channel.layoutTab(*tab,#channel)

  DEFINE_TAB(exec);
  DEFINE_TAB(status);
  DEFINE_TAB(notify);
  DEFINE_TAB(console);
  DEFINE_TAB(target);
  DEFINE_TAB(log);
#undef DEFINE_TAB

  exec.fmt.setForeground(QBrush(Qt::darkRed));
  status.fmt.setForeground(QBrush(Qt::darkBlue));
  notify.fmt.setForeground(QBrush(QColor::fromRgb(0,40,60))); // basically darker cyan
  console.fmt.setForeground(QBrush(QColor::fromRgb(80,60,10))); // basically orange
  target.fmt.setForeground(QBrush(Qt::darkGray));
  log.fmt.setForeground(QBrush(Qt::darkMagenta));

  createPopupWidget();
  tab->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tab, &QWidget::customContextMenuRequested, this, [&](const QPoint& pos){ visibilityPopup.move(pos.x()-5, pos.y()-5); visibilityPopup.show(); });

  onCheckboxesUpdated(-1);

  QWidget* clientWidget = this->widget();
  auto oldlyt = clientWidget->layout();
  oldlyt->removeWidget(outEdit);
  oldlyt->removeWidget(inEdit);

  tab->insertTab(0,outEdit,"main");

  QWidget* newClientWidget = new QWidget();
  auto lyt = new QVBoxLayout();
  lyt->addWidget(tab);
  lyt->addWidget(inEdit);
  lyt->setMargin(1);
  lyt->setSpacing(1);
  newClientWidget->setLayout(lyt);

  this->setWidget(newClientWidget);

  delete clientWidget;
}


void OutputChannel::layoutPopupMenu(QWidget &ctxmenu, QGridLayout &layout, QString name, QString tooltipText, int &counter)
{
  routeChk = new QCheckBox();
  routeChk->setToolTip("Set to either:\n    no output (unchecked)\n    merged into main output (solid)\n    separate tab (checked)");
  routeChk->setTristate(true);
  timestampChk = new QCheckBox();
  timestampChk->setToolTip("Append timestamp to specified channel output");
  auto lbl = new QLabel(name);
  lbl->setToolTip(tooltipText);
  lbl->setFont(QFont("Monospace", 5));
  layout.addWidget(lbl, counter, 0);
  layout.addWidget(routeChk, counter, 1);
  layout.addWidget(timestampChk, counter, 2);
  counter++;
}

void OutputChannel::connectHandler(QMIDock* dock)
{
  QObject::connect(routeChk, &QCheckBox::stateChanged, dock, &QMIDock::onCheckboxesUpdated);
}

void QMIDock::createPopupWidget()
{
  QWidget& widget = visibilityPopup;
  auto layout = new QGridLayout();
  layout->setSpacing(0);
  layout->setMargin(0);
  layout->setColumnMinimumWidth(0, 96);

  QPushButton* close = new QPushButton();
  layout->addWidget(close,0,2);
  close->setText("⌧");
  close->setDefault(true);
  close->setMaximumSize(16,16);
  QObject::connect(close, &QPushButton::pressed, [&]{ visibilityPopup.hide(); });

  QPalette hdrPalette;
  hdrPalette.setColor(QPalette::ColorRole::Window,QColor::fromRgb(255,255,255));
  auto routelbl = new QLabel("⛜");
  auto tslbl = new QLabel("⏲");
  auto namelbl = new QLabel("<u>Name</u>");
  QFont mono("Monospace", 6);
  QFont symbola("Symbola", 10);
  namelbl->setPalette(hdrPalette);
  routelbl->setPalette(hdrPalette);
  tslbl->setBackgroundRole(QPalette::Window);
  routelbl->setBackgroundRole(QPalette::Window);
  tslbl->setBackgroundRole(QPalette::Window);
  namelbl->setFont(mono);
  routelbl->setFont(symbola);
  tslbl->setFont(symbola);
  layout->addWidget(namelbl,1,0);
  layout->addWidget(routelbl,1,1);
  layout->addWidget(tslbl,1,2);

  int i = 2;

#define DEFINE_POPUPMENU(x,tt,cnt)  \
  x.layoutPopupMenu(widget, *layout, #x, tt, cnt)

  DEFINE_POPUPMENU(exec,"shows changes in the running state of the program",i);
  exec.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_POPUPMENU(status,"shows the ongoing progress of long operations",i);
  status.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_POPUPMENU(notify,"tracks the creation of gdb objects such as\nbreakpoints, watchpoints, threads, and so on",i);
  notify.routeChk->setCheckState(Qt::PartiallyChecked);
  DEFINE_POPUPMENU(console,"human-readable output in response to a CLI command",i);
  console.routeChk->setCheckState(Qt::Checked);
  DEFINE_POPUPMENU(target,"produced by the tracee program itself",i);
  target.routeChk->setCheckState(Qt::Unchecked);
  DEFINE_POPUPMENU(log,"comes from gdb's internals such as errors and warnings",i);
  log.routeChk->setCheckState(Qt::Checked);
#undef DEFINE_POPUPMENU
// The following must be in a second stage or the setCheckState calls above will trigger signals before later.
#define CONNECT_CHANNEL(x) \
  connect(x.routeChk, &QCheckBox::stateChanged, \
          this, &QMIDock::onCheckboxesUpdated)

  CONNECT_CHANNEL(exec);
  CONNECT_CHANNEL(status);
  CONNECT_CHANNEL(notify);
  CONNECT_CHANNEL(console);
  CONNECT_CHANNEL(target);
  CONNECT_CHANNEL(log);
#undef CONNECT_CHANNEL

  widget.setLayout(layout);
  widget.setParent(tab);
  widget.setVisible(false);
  widget.setAutoFillBackground(true);

}

void QMIDock::mousePressEvent(QMouseEvent *event)
{
  QWidget* widgetAt = this->childAt(event->x(), event->y());
  if (widgetAt != &visibilityPopup && visibilityPopup.isVisible()) {
    visibilityPopup.setVisible(false);
  }
  QTerminalDock::mousePressEvent(event);
}

QMIDock::QMIDock(QWidget* parent)
  : QMIDock()
{
  this->setParent(parent);
}

QMIDock::~QMIDock()
{
  if (tab) {
    tab->clear();
  }
}

void QMIDock::onContextMenuEvent (QContextMenuEvent *event)
{
  visibilityPopup.move(event->x()-2, event->y()-2);
  visibilityPopup.show();
}

void OutputChannel::handleEvent(QTextEvent& event, QPlainTextEdit &mainEdit, QMIDock* dock, void (QMIDock::*sigfunc)(const QString &))
{
  QPlainTextEdit* editctl;
  Qt::CheckState chkstate = routeChk->checkState();
  if (chkstate == Qt::Checked) {
    editctl = edit;
  } else if (chkstate == Qt::PartiallyChecked) {
    editctl = &mainEdit;
  }
  if (editctl) {
    QTextCharFormat saved(editctl->currentCharFormat());
    editctl->setCurrentCharFormat(fmt);
    if (timestampChk->checkState() == Qt::Checked) {
       struct timespec ts;
       char timestamp[1024];
       clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
	   snprintf(timestamp, 1023, "%ld.%06ld ", ts.tv_sec, ts.tv_nsec/1000);
       editctl->insertPlainText(QLatin1String(timestamp));
    }
    editctl->insertPlainText(event.text);
    editctl->setCurrentCharFormat(saved);
  }
  (dock->*sigfunc)(event.text);
}


void QMIDock::execText (const QTextEvent &text)
{

}

void QMIDock::statusText (const QTextEvent &text)
{

}

void QMIDock::notifyText (const QTextEvent &text)
{

}

void QMIDock::consoleText (const QTextEvent &text)
{

}

void QMIDock::targetText (const QTextEvent &text)
{

}

void QMIDock::logText(const QTextEvent &text)
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

