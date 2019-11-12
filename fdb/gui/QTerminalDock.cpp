#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <stdlib.h>
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
#include "gui/QTerminalIOEvent.h"
#include "gui/QTerminalDock.h"
#include "io/EPollDispatcher.h"
#include "util/exceptions.hpp"
#include "util/log.hpp"
#include "util/safe_deque.hpp"

QEvent::Type QTerminalDock::ioEventType;

QTerminalDock::QTerminalDock()
  : QOptionsDock (), ioview(OUT_ONLY),
    lszbtn(nullptr), lszedit(nullptr),
    freezeChk(nullptr),
    inEdit(nullptr), outEdit(nullptr),
    _ptfd(),
    _iothread(nullptr),
    outbuf_idx(0),
    outbuf_siz(0)
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
  // TODO unregister EpollDispatch listener
  destroyPty();
}


const QString QTerminalDock::ptyname()
{
   return QString(::ptsname(*_ptfd));
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


void QTerminalDock::setIoView (QTerminalDock::IoView _view)
{
  if (_view == IoView::IN_AND_OUT) {
    inEdit->show();
  } else {
    inEdit->hide();
  }
  outEdit->setReadOnly(_view != IoView::COMBINED);
}


void QTerminalDock::registerEventTypes ()
{
  ioEventType = static_cast<QEvent::Type>(QEvent::registerEventType(-1));
}


bool QTerminalDock::event(QEvent *e)
{
  if (e->type() == ioEventType) {
    if (auto qte = dynamic_cast<QTerminalIOEvent*>(e); qte) {
      onTerminalEvent(*qte);
    }
  }
  return QWidget::event(e);
}


void QTerminalDock::createPty()
{
  struct winsize win = terminalDimensions();

  // problematic, seems to randomize certain ty behaviours
  int fd = getpt();
  if (fd == -1) errno_runtime_error;
  _ptfd = std::make_shared<autoclosing_fd>(fd);

  int r = ioctl(*_ptfd, TIOCSWINSZ, &win);
  assert_re(r==0, "Couldn't set window size on the pseudoterminal (%s)", strerror(errno));
  unlockpt(*_ptfd);

  auto appioDispatch = EPollDispatcher::def();
  using namespace std::placeholders;
  appioDispatch->start_listen(_ptfd, EPOLLIN, std::bind(&QTerminalDock::onEPollIn, this, _1));
  appioDispatch->start_listen(_ptfd, EPOLLOUT, std::bind(&QTerminalDock::onEPollOut, this, _1));

}


void QTerminalDock::destroyPty()
{
  // actually closing the fd is the sole responsibility of the slave
  auto appioDispatch = EPollDispatcher::def();
  if (_ptfd) {
    appioDispatch->stop_listen(*_ptfd);
    _ptfd.reset();
  }

}


EPollListener::ret_t QTerminalDock::onEPollIn (const struct epoll_event& eev) try
{
  int r = static_cast<int>(::read(eev.data.fd, inbuf+inbuf_idx, static_cast<size_t>(inbuf_max_siz - inbuf_idx - 1)));
  if (r == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
    Logger::error("While trying to read data from a pty: %s", strerror(errno));
    return EPollListener::ret_t{ true, 0, 0 };
  }
  if (errno == EWOULDBLOCK || errno == EAGAIN) {
    // Basically, there are no bytes available. (epoll should prevent this). Just return.
    return EPollListener::ret_t{ true, 0, 0 };
  }

  // Append the new null terminator
  inbuf[inbuf_idx+r] = 0;

  // scan for a new linefeed in the new portion of the buffer
  int i=0;
  for (i=inbuf_idx; i<r && inbuf[i] != '\n'; i++) { }
  inbuf_idx += r;

  do {
    QByteArray ba(inbuf, i+1);
    auto text = QString::fromLatin1(ba);
    auto tioev = new QTerminalIOEvent(std::move(text));
    strncpy(inbuf, inbuf+i+1, static_cast<size_t>(r-i-1));
    inbuf_idx -= (i+1);
    QCoreApplication::postEvent(this, tioev);

    // if there is remaining text, scan for a new line from the start
    if (inbuf_idx == 0) {
      break;
    } else {
      for (i=0; i<inbuf_idx && inbuf[i] != '\n'; i++) { }
    }
  } while(1);

  return EPollListener::ret_t{ true, 0, 0 };

} catch (std::exception& e) {
  char msg[1024];
  snprintf(msg, 1023, "Critical exception in epoll thread during QTerminalDock i/o: %s", e.what());
  Logger::critical(msg);
  auto ev = new QTerminalIOEvent(QLatin1String(msg));
  QCoreApplication::postEvent(this,ev);
  return EPollListener::ret_t{true,0,0};
}


EPollListener::ret_t QTerminalDock::onEPollOut (const struct epoll_event& eev)
try {
  EPollListener::ret_t ret;
  inputQueue.fast_lock();
  if (outbuf_idx < outbuf_siz) {
    ssize_t rw = ::write(*_ptfd, outbuf + outbuf_idx, static_cast<unsigned long>(outbuf_siz - outbuf_idx));
    if (rw == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      // could theoretically happen with a nonblocking socket i suppose, but epoll is supposed to prevent this
      return EPollListener::ret_t{ true, 0, 0 };
    }
    assert_re(rw != -1, "Write error while performing continued write to a pseudoterminal descriptor: %s", strerror(errno));
    outbuf_idx += rw;
    ret.handled = true;
  } else if (inputQueue.size() > 0) {
    const QString* qs = inputQueue.pop_back();
    strncpy(outbuf, qs->toLatin1().data(), outbuf_max_siz);
    outbuf_idx = 0;
    outbuf_siz = qs->size();
    ssize_t rw = ::write(*_ptfd, outbuf + outbuf_idx, static_cast<unsigned long>(outbuf_siz - outbuf_idx));
    assert_re(rw != -1, "Write error while writing to a pseudoterminal descriptor: %s", strerror(errno));
    outbuf_idx += rw;
    QString disp = *qs;
    disp = disp.replace('\r', "\\r");
    disp = disp.replace('\n', "\\n");
    Logger::debug("Input sent: %s", disp.toLatin1().data());
    delete qs;
    ret.handled = true;
  }
  if (inputQueue.empty() && outbuf_idx >= outbuf_siz) {
    ret.rem |= EPOLLOUT;
  }
  inputQueue.fast_unlock();
  return ret;
} catch (std::exception& e) {
  char msg[1024];
  snprintf(msg, 1023, "Critical exception in epoll thread during QTerminalDock i/o: %s", e.what());
  Logger::critical(msg);
  auto ev = new QTerminalIOEvent(QLatin1String(msg));
  QCoreApplication::postEvent(this,ev);
  return EPollListener::ret_t{true,0,0};
}


void QTerminalDock::windowSizeChanged (const struct winsize& newSize)
{
  int r = ioctl(*_ptfd, TIOCGWINSZ, &newSize);
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


void QTerminalDock::onTerminalEvent (QTerminalIOEvent& event)
{
  // this signal will be emitted by the new QTerminalDock instead
  output(event);
  if (event.display) {
    outEdit->insertPlainText(event.text);
    if (freezeChk->checkState() == Qt::CheckState::Unchecked) {
      outEdit->ensureCursorVisible();
    }
  }
}

