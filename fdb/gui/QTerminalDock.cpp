#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pty.h>
#include <utility>
#include <QPushButton>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QThread>
#include <QScrollBar>
#include "gui/QTerminalIOEvent.h"
#include "gui/QTerminalDock.h"
#include "util/exceptions.hpp"
#include "util/log.hpp"
#include "util/safe_deque.hpp"

QEvent::Type QTerminalDock::ioEventType;

QTerminalDock::QTerminalDock()
  : QOptionsDock (), ioview(OUT_ONLY),
    lszbtn(nullptr), lszedit(nullptr),
    freezeChk(nullptr),
    inEdit(nullptr), outEdit(nullptr),
    _ptfd(0), _epoll(0),
    _iothread(nullptr)
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
  outEdit->setCenterOnScroll(true);

  inEdit = new QLineEdit(clientWidget);
  inEdit->setFont(trmfont);
  inEdit->setMaximumHeight(24);
  inEdit->setMinimumHeight(24);

  lyt->addWidget(outEdit);
  lyt->addWidget(inEdit);

  clientWidget->setLayout(lyt);
  setWidget(clientWidget);
  clientWidget->show();

  connect(inEdit, &QLineEdit::returnPressed, this, &QTerminalDock::returnPressed);

}


QTerminalDock::QTerminalDock (QWidget *parent)
  : QTerminalDock()
{
  this->setParent(parent);
}


QTerminalDock::~QTerminalDock ()
{
  if (_iothread) {
    stopIOThread();
  }

  _ptfd = 0;
  destroyPty();

  if (_epoll) {
    ::close(_epoll);
    _epoll = 0;
  }
}

const QString QTerminalDock::ptyname()
{
   return QString(::ptsname(_ptfd));
}

struct winsize QTerminalDock::terminalDimensions ()
{
  QLayout* lyt = this->widget()->layout();

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
  ioEventType = static_cast<QEvent::Type>(QEvent::registerEventType(-1));
}


bool QTerminalDock::event(QEvent *e)
{
  if (e->type() == ioEventType) {
    auto qte = static_cast<QTerminalIOEvent*>(e);
    onTerminalOutput(qte->text);
  }
  return QWidget::event(e);
}


void QTerminalDock::createPty()
{
  destroyPty();
  struct winsize win = terminalDimensions();

  // problematic, seems to randomize certain ty behaviours
  _ptfd = getpt();
  if (_ptfd == -1) errno_runtime_error;

  int r = ioctl(_ptfd, TIOCSWINSZ, &win);
  assert_re(r==0, "Couldn't set window size on the pseudoterminal (%s)", strerror(errno));

  unlockpt(_ptfd);

  _epoll = epoll_create(1);
  struct epoll_event eev;
  memset(&eev, 0, sizeof(struct epoll_event));
  eev.events |= EPOLLIN;
  eev.data.fd = _ptfd;
  r = epoll_ctl(_epoll, EPOLL_CTL_ADD, _ptfd, &eev);
  assert_re(r==0, "Error during EPOLL_CTL_ADD: %s", strerror(errno));
}

void QTerminalDock::destroyPty()
{
  // actually closing the fd is the sole responsibility of the slave
  _ptfd = 0;
}

void QTerminalDock::startIOThread()
{
  if (_iothread) {
    Logger::warning("Tried to create/start QTerminalDock::_iothread, but it already has a value.");
  }
  _iothread = QThread::create(std::bind(&QTerminalDock::performIO, this));
  _iothread->start();
}

void QTerminalDock::stopIOThread()
{
  if (!_iothread) {
    Logger::warning("Tried to stop _iothread but it is nullptr");
    return;
  }
  if(!_iothread->isRunning()) {
    Logger::warning("Tried to stop _iothread but it isn't even running!");
  }

  _iothread->terminate();

  if (_iothread->wait(2000)) {
    delete _iothread;
    _iothread = nullptr;
  }
}

void QTerminalDock::performIO()
{
  int r = 0;
  long n = 0; // bytes read
  struct epoll_event eev;
  char rdbuf[4096];

  while (1) {
    try {
      r = epoll_wait(_epoll, &eev, 1, -1);
      if (r && (eev.events & EPOLLIN)) {
        n = ::read(eev.data.fd, rdbuf, 4095);
        rdbuf[n] = 0;
        QString text = QString::fromLatin1(rdbuf,static_cast<int>(n));
        auto ev = new QTerminalIOEvent(std::forward<QString>(text));
        QCoreApplication::postEvent(this, ev);
      } else if (r && (eev.events & EPOLLOUT)) {
        inputQueue.fast_lock();
        if (inputQueue.size() > 0) {
          const QString* qs = inputQueue.pop_back();
          int written = 0;
          while (written < qs->size()) {
            ssize_t rw = ::write(_ptfd, qs->toLatin1().data() + written, static_cast<size_t>(qs->size()));
            assert_re(rw != -1, "Write error while writing to a pseudoterminal descriptor: %s", strerror(errno));
            written += rw;
          }
          QString disp = *qs;
          disp = disp.replace('\r', "\\r");
          disp = disp.replace('\n', "\\n");
          Logger::debug("Input sent: %s", disp.toLatin1().data());
          delete qs;
        }        
        if (inputQueue.empty()) {
          struct epoll_event eev2;
          memset(&eev2, 0, sizeof(struct epoll_event));
          eev2.events = EPOLLIN; // i.e.: removing EPOLLOUT
          eev2.data.fd = _ptfd;
          epoll_ctl(_epoll, EPOLL_CTL_MOD, _ptfd, &eev2);
        }
        inputQueue.fast_unlock();
      } else if (r == -1 && errno == EINTR) {
        // we get here following debugger breakpoint activity
        // but also during external termination
      }

    } catch (const std::exception& e) {
      char msg[1024];
      snprintf(msg, 1023, "Critical exception in IOThread: %s", e.what());
      Logger::critical(msg);
      QString text(msg);
      auto ev = new QTerminalIOEvent(std::forward<QString>(text));
      QCoreApplication::postEvent(this,ev);
    }
  }
}

void QTerminalDock::windowSizeChanged (const struct winsize& newSize)
{
  int r = ioctl(_ptfd, TIOCGWINSZ, &newSize);
  if (r) throw errno_runtime_error;
}

void QTerminalDock::windowSizeChanged (unsigned short rows, unsigned short cols)
{
  struct winsize ws;
  memset(&ws,0,sizeof(struct winsize));
  ws.ws_col = cols;
  ws.ws_row = rows;
  windowSizeChanged(ws);
}

void QTerminalDock::write (const char *fmt, ...)
{
  char* bufr = reinterpret_cast<char*>(alloca( 4096 ));
  va_list vargs;
  va_start(vargs,fmt);
  vsnprintf(bufr, 4096, fmt, vargs);
  va_end(vargs);
  QString qs = QString::fromLatin1(bufr);
  writeInput(std::move(qs));
}



void QTerminalDock::returnPressed()
{
  QString text = inEdit->text();
  if (text.trimmed().size() == 0) {
    inEdit->clear();
    return;
  }
  inEdit->clear();
  writeInput(text);
  input(text);
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

void QTerminalDock::onTerminalOutput (const QString &qs)
{
  // this signal will be emitted by the new QTerminalDock instead
  output(qs);
  outEdit->insertPlainText(qs);
  if (freezeChk->checkState() == Qt::CheckState::Unchecked) {
    outEdit->ensureCursorVisible();
  }
}

