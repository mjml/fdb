#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <pty.h>
#include "util/log.hpp"
#include <QCoreApplication>
#include <QThread>
#include <QFontMetrics>
#include "AsyncPty.h"


AsyncPty::AsyncPty(const QString& name, QObject* parent)
  : QObject(parent),
    _name(name),
    _ptfd(),
    outbuf_idx(0),
    outbuf_siz(0),
    inbuf_idx(0)
{

}

AsyncPty::~AsyncPty()
{
}

const QString AsyncPty::ptyname()
{
  return QLatin1String(::ptsname(_ptfd->fd));
}


bool AsyncPty::event(QEvent *e)
{
  if (e->type() == QTextEvent::ioEventType) {
    if (auto qte = dynamic_cast<QTextEvent*>(e); qte) {
      dispatchEvent(*qte);
      return true;
    } else {
      Logger::warning("AsyncPty received a QEvent that was not a QTextEvent.");
    }
  }
  return true;
}

void AsyncPty::write (QString& qs)
{
  if (!_ptfd)  return;
  if (!qs.endsWith(('\n'))) qs += '\n';

  // fire signal
  QTextEvent qte(QTextEvent::Type::Input, QString(qs));
  input(qte);

  auto appioDispatch = EPollDispatcher::def();
  inputQueue.safe_emplace_front(qs);
  appioDispatch->modify(*_ptfd, EPOLLOUT, 0);
}

void AsyncPty::write (const char *fmt, ...)
{
  char* bufr = reinterpret_cast<char*>(alloca( 4096 ));
  va_list vargs;
  va_start(vargs,fmt);
  vsnprintf(bufr, 4096, fmt, vargs);
  va_end(vargs);
  QString qs = QString::fromLatin1(bufr);
  write(qs);
}

void AsyncPty::setTerminalDimensions (int width, int height, int xpixel, int ypixel)
{
  if (!_ptfd) return;

  struct winsize win;  memset(&win, 0, sizeof(winsize));
  win.ws_col = static_cast<unsigned short>(width);
  win.ws_row = static_cast<unsigned short>(height);
  win.ws_xpixel = static_cast<unsigned short>(xpixel);
  win.ws_ypixel = static_cast<unsigned short>(ypixel);

  int r = ioctl(*_ptfd, TIOCGWINSZ, &win);
  if (r) throw errno_runtime_error;
}


void AsyncPty::createPty()
{
  int fd = getpt();
  if (fd == -1) errno_runtime_error;
  _ptfd = std::make_shared<vanilla_fd>(fd);

  int cols=80, rows=40, xpixel=16, ypixel=20;
  struct winsize win;
  needsTerminalDimensions(cols, rows, xpixel, ypixel);
  win.ws_row = static_cast<uint16_t>(rows);
  win.ws_col = static_cast<uint16_t>(cols);
  win.ws_xpixel = static_cast<uint16_t>(xpixel);
  win.ws_ypixel = static_cast<uint16_t>(ypixel);
  int r = ioctl(*_ptfd, TIOCSWINSZ, &win);
  assert_re(r==0, "Couldn't set window size on the pseudoterminal (%s)", strerror(errno));
  unlockpt(*_ptfd);

  struct termios tmios;
  r = tcgetattr(*_ptfd, &tmios);
  assert_re(r != -1, "Couldn't get terminal settings on pty: %s", strerror(errno));
  tmios.c_lflag &= static_cast<unsigned int>(~(ECHO));
  r = tcsetattr(*_ptfd, TCSANOW, &tmios);
  assert_re(r != -1, "Couldn't set terminal settings on pty: %s", strerror(errno));

  QTextEvent qte (QTextEvent::Opened);
  status(qte);

  auto appioDispatch = EPollDispatcher::def();
  using namespace std::placeholders;
  appioDispatch->start_listen(_ptfd, EPOLLIN, std::bind(&AsyncPty::onEPollIn, this, _1));
  appioDispatch->start_listen(_ptfd, EPOLLOUT, std::bind(&AsyncPty::onEPollOut, this, _1));
  appioDispatch->start_listen(_ptfd, EPOLLHUP, std::bind(&AsyncPty::onEPollHup, this, _1));

}


void AsyncPty::destroyPty()
{
  // actually closing the fd is the sole responsibility of the slave
  auto appioDispatch = EPollDispatcher::def();
  if (_ptfd) {
    appioDispatch->stop_listen(*_ptfd);
    _ptfd.reset();
  }
  auto qte = new QTextEvent(QTextEvent::Closed);
  QCoreApplication::postEvent(this,qte);
}


EPollListener::ret_t AsyncPty::onEPollIn (const struct epoll_event& eev) try
{
  int r = static_cast<int>(::read(eev.data.fd, inbuf+inbuf_idx, static_cast<size_t>(inbuf_max_siz - inbuf_idx - 1)));
  if (r == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      // Basically, there are no bytes available. (epoll should prevent this). Just return.
      Logger::warning("EWOULDBLOCK received on dock for %s (inbuf_idx=%d)", _name.toLatin1().data(), inbuf_idx);
      return EPollListener::ret_t{ true, 0, 0 };
    } else {
      Logger::error("While trying to read data from a pty: %s", strerror(errno));
      return EPollListener::ret_t{ true, 0, 0 };
    }
  }

  // Append the new null terminator
  inbuf[inbuf_idx+r] = 0;

  // Scan for a new linefeed in the new portion of the buffer
  int i=0;
  for (i=inbuf_idx; i<r && inbuf[i] != '\n'; i++) { }
  if (!inbuf[i]) {
    i--; // if null terminator was found, back up one
  }
  inbuf_idx += r;

  int loopcnt = 0;

  do {
    auto text = QString::fromLatin1(inbuf, i+1);
    bool accept = text.endsWith('\n');
    if (!accept) {
      for (auto it = accept_prefixes.begin(); it != accept_prefixes.end(); it++) {
        if (text.endsWith(*it)) {
          accept = true;
          break;
        }
      }
    }

    if (accept) {
      //Logger::debug("Output recvd: %s", text.toLatin1().data());
      auto tioev = new QTextEvent(QTextEvent::Output, std::move(text));
      QCoreApplication::postEvent(this, tioev);
      for (int j=0; inbuf[i+1+j] && i+1+j < inbuf_idx; j++) {
        inbuf[j] = inbuf[i+1+j];
      }
      inbuf_idx -= (i+1);

    } else {
      Logger::debug("Returning with %d bytes in buffer (loopcnt=%d): \n[[%s]]\n[[%s]]", inbuf_idx, loopcnt, inbuf, text.toLatin1().data());
      break;
    }

    // If there is remaining text, scan for a new line from the start
    if (inbuf_idx == 0) {
      break;
    } else {
      for (i=0; i<inbuf_idx && inbuf[i] != '\n'; i++) { } // find the next linefeed
      if (i==inbuf_idx) i--; // if null terminator was found instead, back up one
    }
    if (++loopcnt % 100 == 0) {
      Logger::warning("Looped way too many times while handling input.");
    }
  } while(1);

  return EPollListener::ret_t{ true, 0, 0 };

} catch (std::exception& e) {
  char msg[1024];
  snprintf(msg, 1023, "Critical exception in epoll thread during AsyncPty i/o: %s", e.what());
  Logger::critical(msg);
  auto ev = new QTextEvent(QTextEvent::Type::Error, QLatin1String(msg));
  QCoreApplication::postEvent(this,ev);
  return EPollListener::ret_t{true,0,0};
}


#pragma GCC diagnostic ignored "-Wunused-parameter"
EPollListener::ret_t AsyncPty::onEPollOut (const struct epoll_event& eev) try {
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
  snprintf(msg, 1023, "Critical exception in epoll thread during AsyncPty i/o: %s", e.what());
  Logger::critical(msg);
  auto ev = new QTextEvent(QTextEvent::Type::Error, QLatin1String(msg));
  QCoreApplication::postEvent(this,ev);
  return EPollListener::ret_t{true,0,0};
}


EPollListener::ret_t AsyncPty::onEPollHup (const epoll_event &eev)
{
  // fire signal
  auto qte = new QTextEvent(QTextEvent::Hangup);
  QCoreApplication::postEvent(this,qte);
  // take down the pty
  destroyPty();
  return EPollListener::ret_t{true, 0, 0};
}

void AsyncPty::dispatchEvent(QTextEvent &event)
{
  switch (event.ttype)
  {
  case QTextEvent::Type::Input:
    input(event);
    break;
  case QTextEvent::Type::Output:
    output(event);
    break;
  case QTextEvent::Type::Error:
    error(event);
    break;
  case QTextEvent::Opened:
  case QTextEvent::Closed:
  case QTextEvent::Hangup:
    status(event);
    break;
  case QTextEvent::Noop:
    ;
  }
  // if unrecognized, just return -- derived classes may create more event types
}
