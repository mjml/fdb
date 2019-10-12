#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>
#include <QPushButton>
#include <QBoxLayout>
#include <QCoreApplication>
#include "gui/QTerminalIOEvent.h"
#include "gui/QTerminalDock.h"
#include "util/exceptions.hpp"

QEvent::Type QTerminalDock::terminalOutputEventType;

QTerminalDock::QTerminalDock()
  : QOptionsDock (), lszbtn(nullptr), lszedit(nullptr),
    freezeChk(nullptr),
    outEdit(nullptr), inEdit(nullptr),
    _process(nullptr), ioview(OUT_ONLY)
{
  QFont mini(QStringLiteral("Monospace"),5);

  setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
  setAllowedAreas(Qt::DockWidgetArea::TopDockWidgetArea | Qt::DockWidgetArea::BottomDockWidgetArea);

  // title bar
  lszbtn = new QPushButton(titleBar);
  lszbtn->setFont(mini);
  lszbtn->setStyleSheet("background-color: #808080");
  lszbtn->setText(QStringLiteral("inf"));
  lszbtn->setMaximumWidth(48);
  lszbtn->setCheckable(true);
  lszbtn->setChecked(true);

  QObject::connect (lszbtn,    &QPushButton::clicked,
                    this,      &QTerminalDock::logSizeLabelClicked);

  lszedit = new QLineEdit(titleBar);
  lszedit->setFont(mini);
  lszedit->setMaximumWidth(48);
  lszedit->setMaximumHeight(20);
  lszedit->setMaxLength(7);
  lszedit->setPlaceholderText(QStringLiteral("numlines"));
  lszedit->setInputMask("0000000;");
  lszedit->setAlignment(Qt::AlignCenter);
  lszedit->setStyleSheet("background:grey");
  lszedit->hide();

  QObject::connect (lszedit,   &QLineEdit::editingFinished,
                    this,      &QTerminalDock::logSizeEdited);

  freezeChk = new QCheckBox(titleBar);
  freezeChk->setFont(mini);
  freezeChk->setText("lock");

  auto tblyt = dynamic_cast<QHBoxLayout*>(titleBar->layout());
  tblyt->addWidget(lszbtn);
  tblyt->addWidget(lszedit);
  tblyt->addWidget(freezeChk);
  tblyt->addStretch(1);

  // client area
  auto lyt = new QVBoxLayout();
  lyt->setMargin(4);
  QFont trmfont("Monospace", 7);

  auto clientWidget = new QWidget(this);
  outEdit = new QPlainTextEdit(clientWidget);
  outEdit->setReadOnly(true);
  outEdit->setFont(trmfont);
  outEdit->setMinimumHeight(32);

  inEdit = new QLineEdit(clientWidget);
  inEdit->setFont(trmfont);
  inEdit->setMaximumHeight(24);
  inEdit->setMinimumHeight(24);

  lyt->addWidget(outEdit);
  lyt->addWidget(inEdit);

  clientWidget->setLayout(lyt);
  setWidget(clientWidget);
  clientWidget->show();
  show();
}

QTerminalDock::QTerminalDock (QWidget *parent)
  : QTerminalDock()
{
  this->setParent(parent);
}

QTerminalDock::~QTerminalDock ()
{

}

struct winsize QTerminalDock::terminalDimensions ()
{
  QLayout* lyt = this->layout();

  for (int i=0; i < lyt->count(); i++) {
    QWidget* editctl = lyt->itemAt(i)->widget();
    if (!editctl) continue;
    outEdit = dynamic_cast<QPlainTextEdit*>(editctl);
    if (outEdit) break;
  }
  if (!outEdit) {
    Throw(std::logic_error,"Expected to find a QPlainTextEdit inside this QTerminalWidget");
  }

  auto chfmt = outEdit->currentCharFormat();
  auto fnt = chfmt.font();
  auto metrics = QFontMetrics(fnt);

  struct winsize win;  memset(&win, 0, sizeof(winsize));
  win.ws_col = static_cast<unsigned short>(outEdit->width() / metrics.horizontalAdvance(QChar(' ')));
  win.ws_row = static_cast<unsigned short>(outEdit->height() / metrics.height());
  win.ws_xpixel = static_cast<unsigned short>(metrics.horizontalAdvance(QChar(' ')));
  win.ws_ypixel = static_cast<unsigned short>(metrics.height());

  return win;

}

void QTerminalDock::setIoView(QTerminalDock::IoView _view)
{
  if (_view == IoView::IN_AND_OUT) {
    inEdit->show();
  } else {
    inEdit->hide();
  }
  outEdit->setReadOnly(_view != IoView::COMBINED);
}

void QTerminalDock::registerEventTypes()
{
  terminalOutputEventType = static_cast<QEvent::Type>(QEvent::registerEventType(-1));
}

bool QTerminalDock::event(QEvent *e)
{
  if (e->type() == terminalOutputEventType) {
    auto qte = static_cast<QTerminalIOEvent*>(e);
    onTerminalOutput(qte->text);
  }
  return QWidget::event(e);
}

void QTerminalDock::setupEPoll()
{
  _epoll = epoll_create(1);
  struct epoll_event eev;
  memset(&eev, 0, sizeof(struct epoll_event));
  eev.events |= EPOLLIN;

  int r = epoll_ctl(_epoll, EPOLL_CTL_ADD, _fds.fdin, &eev);
  assert_re(r==0, "Error during EPOLL_CTL_ADD: %s", strerror(errno));
}

void QTerminalDock::performIO()
{
  int r = 0;
  int n = 0; // bytes read
  struct epoll_event eev;
  char rdbuf[4096];
  while (1) {
    try {
      r = epoll_wait(_epoll, &eev, 1, -1);
      if (r && (eev.events & EPOLLIN)) {
        n = read(_fds.fdin, rdbuf, 4095);
        rdbuf[n] = 0;
        QString text = QString::fromLatin1(rdbuf,n);
        auto ev = new QTerminalIOEvent(std::forward<QString>(text));
        QCoreApplication::postEvent(this, ev);
      }

    } catch (const std::exception& e) {
      QString text(e.what());
    }

  }
}

void QTerminalDock::logSizeLabelClicked (bool checked)
{
  if (checked) {
    lszedit->hide();
    lszbtn->show();
  } else {
    lszbtn->hide();
    lszedit->setText("");
    lszedit->setCursorPosition(0);
    lszedit->show();
    lszedit->setFocus();
  }
}

void QTerminalDock::logSizeEdited ()
{
  QString qs = lszedit->text();

  // TODO: store this size somewhere, or modify the edit controls
  auto sz = qs.toInt();
  if (sz < 10) {
    sz = 0;
    lszedit->hide();
    lszbtn->setChecked(true);
    lszbtn->show();
  } else {
    lszedit->setStyleSheet("background-color: #808080");
    this->setFocus();
  }

}

void QTerminalDock::onTerminalOutput (const QString &qs)
{
  output(qs);
  outEdit->insertPlainText(qs);
}

void QTerminalDock::onErrorOccurred (QProcess::ProcessError error)
{

}

void QTerminalDock::onFinished (int exitcode, QProcess::ExitStatus status)
{

}

void QTerminalDock::onStateChanged (QProcess::ProcessState newState)
{

}

