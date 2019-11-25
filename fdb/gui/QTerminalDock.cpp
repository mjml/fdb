#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <pty.h>
#include <QPushButton>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QThread>
#include <QScrollBar>
#include <utility>
#include <functional>
#include "util/QTextEvent.h"
#include "gui/QTerminalDock.h"
#include "io/EPollDispatcher.h"
#include "util/exceptions.hpp"
#include "util/log.hpp"
#include "util/safe_deque.hpp"


QTerminalDock::QTerminalDock()
  : ioview(OUT_ONLY),
    lszbtn(nullptr), lszedit(nullptr),
    freezeChk(nullptr),
    inEdit(nullptr), outEdit(nullptr)
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
  lyt->setMargin(1);
  lyt->setSpacing(1);
  QFont trmfont("Monospace", 7);

  auto clientWidget = new QWidget(this);
  outEdit = new QPlainTextEdit();
  outEdit->setReadOnly(true);
  outEdit->setFont(trmfont);
  outEdit->setMinimumHeight(32);
  outEdit->setCenterOnScroll(true);

  inEdit = new QLineEdit();
  inEdit->setFont(trmfont);
  inEdit->setMaximumHeight(24);
  inEdit->setMinimumHeight(24);

  lyt->addWidget(outEdit);
  lyt->addWidget(inEdit);

  clientWidget->setLayout(lyt);
  setWidget(clientWidget);
  clientWidget->show();

  QObject::connect (inEdit,    &QLineEdit::returnPressed,
                    this,      &QTerminalDock::returnPressed);

}


QTerminalDock::QTerminalDock (QWidget *parent)
  : QTerminalDock()
{
  this->setParent(parent);
}


QTerminalDock::~QTerminalDock ()
{
  Logger::debug2("Destroying a QTerminalDock: %s", this->objectName().toLatin1().data());
}


void QTerminalDock::setIoView (QTerminalDock::IoView _view)
{
  if (_view == IoView::IN_AND_OUT) {
    inEdit->show();
  } else {
    inEdit->hide();
  }
  outEdit->setReadOnly(_view != IoView::COMBINED);
}

void QTerminalDock::output (QTextEvent &event)
{
  if (event.display) {
    outEdit->insertPlainText(event.text);
    if (freezeChk->checkState() == Qt::CheckState::Unchecked) {
      outEdit->ensureCursorVisible();
    }
  }
}

#pragma GCC diagnostic ignored "-Wswitch"
void QTerminalDock::status (QTextEvent &event)
{
  switch (event.ttype) {
  case QTextEvent::Opened:
  case QTextEvent::Closed:
  case QTextEvent::Hangup:
    ;
  }
}

void QTerminalDock::error (QTextEvent &event)
{
  auto saved = outEdit->currentCharFormat();
  auto fmt = saved;
  fmt.setForeground(QBrush(QColor::fromRgb(100,20,20)));
  fmt.setFont(QFont("Monospace", 10));
  outEdit->setCurrentCharFormat(fmt);
  if (!event.text.endsWith('\n')) {
    QString qs = event.text;
    qs += '\n';
    outEdit->insertPlainText(qs);
  } else {
    outEdit->insertPlainText(event.text);
  }
  outEdit->setCurrentCharFormat(saved);
}


void QTerminalDock::returnPressed ()
{
  QString text = inEdit->text();
  if (text.trimmed().size() == 0) {
    inEdit->clear();
    return;
  }
  inEdit->clear();
  QTextEvent qte(QTextEvent::Input, std::move(text));
  input(qte);
}

void QTerminalDock::provideTerminalDimensions (int &width, int &height, int &xpixel, int &ypixel)
{
  auto fmt = outEdit->currentCharFormat();
  auto fnt = fmt.font();
  auto metrics = QFontMetrics(fnt);

  xpixel = metrics.horizontalAdvance(QChar(' '));
  ypixel = metrics.height();
  width = outEdit->width() / xpixel;
  height = outEdit->height() / ypixel;
}


void QTerminalDock::resizeEvent (QResizeEvent *event)
{
  auto fmt = outEdit->currentCharFormat();
  auto fnt = fmt.font();
  auto metrics = QFontMetrics(fnt);

  int xpixel = metrics.horizontalAdvance(QChar(' '));
  int ypixel = metrics.height();
  int width = outEdit->width() / xpixel;
  int height = outEdit->height() / ypixel;

  if (height <= 1) {
    event->setAccepted(false);
    return;
  }

  terminal_resize(width, height, xpixel, ypixel);
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



