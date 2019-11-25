#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <mutex>
#include "util/safe_deque.hpp"
#include "util/exceptions.hpp"
#include "util/log.hpp"
#include "EPollDispatcher.h"

std::shared_ptr<EPollDispatcher> EPollDispatcher::appioDispatch;



EPollListener::ret_t SingleClientAcceptor::acceptFunction (const struct epoll_event& eev) {
  int newfd = 0;
  if (eev.events & EPOLLIN) {
    struct sockaddr sa;
    socklen_t len = 0;
    Logger::debug("Accepting incoming connection on fdbsocket: fd: %d", eev.data.fd);

    EPollDispatcher::def()->mut.lock();

    newfd = ::accept(*pfd, &sa, &len);
    if (newfd == -1) {
      Logger::warning("Error while accepting fdbsocket connection. [\"%s\"]", strerror(errno));
      return ret_t{false,0,0};
    }

    int oldfd = *pfd;

    Logger::debug("Accepted client connection on newfd=%d, pivoting...", newfd);
    *pfd = newfd;
    EPollDispatcher::def()->start_listen(pfd, EPOLLIN, deferredHandler);
    EPollDispatcher::def()->stop_listen(oldfd); // will call `delete this', so don't touch the object after this!

    ::close(oldfd);

    EPollDispatcher::def()->mut.unlock();

  } else if (eev.events & EPOLLHUP) {
    // could happen during an incomplete handshake, for example.
    Logger::warning("SingleClientAcceptor's socket received HUP");
  } else if (eev.events & EPOLLRDHUP) {
    Logger::warning("SingleClientAcceptor's socket received RDHUP");
  } else if (eev.events & EPOLLERR) {
    Logger::error("SingleClientAcceptor encountered an error. fd will be close()'d");
    ::close(pfd->fd);
  } else if (eev.events & EPOLLPRI) {
    Logger::fuss("SingleClientAcceptor encountered an exceptional condition for fd=%d", pfd->fd);
  }

  // Must return 0,0 here since the EPOLLIN handler will be calling
  //   stop_listen() which removes this fd from all maps.
  return ret_t { true, 0, 0 };
}


EPollListener::~EPollListener ()
{

}


EPollDispatcher::~EPollDispatcher ()
{
  std::scoped_lock lock(mut);
  for (auto it = listeners.begin(); it != listeners.end(); it++) {
    delete it->second;
  }
  listeners.clear();
  running = false;
  join();
}


void EPollDispatcher::start ()
{
  _epoll = epoll_create(1);
  assert_re(_epoll >= 0, "Couldn't create epoll file descriptor");
  running = true;
  th = new std::thread(std::bind(&EPollDispatcher::run, this));
}


void EPollDispatcher::join ()
{
  running = false;
  if (th) {
    th->join();
    delete th;
    th = nullptr;
  }
  if (_epoll) {
    close(_epoll);
    _epoll = 0;
  }
}


void EPollDispatcher::run ()
{
  int r = 0;
  struct epoll_event eev;

  while (running) {
    eev.data.u64 = 0;
    eev.events = 0;
    r = epoll_wait(_epoll, &eev, 64, 400);

    int fd = eev.data.fd;
    if (r > 0) {
      bool handled = false;
      mut.lock();
      auto bkt = listeners.bucket(fd);
      for (auto it = listeners.begin(bkt); it != listeners.end(bkt); it++) {
        const EPollListener* listener = it->second;
        if ((eev.events & listener->events) && listener->handler) {
          auto p = listener->handler(eev);
          handled = true;  // note difference between handled and p.handled
          if (p.add|| p.rem) {
            modify(fd, p.add, p.rem);
            if (p.handled) break; // if the listener signals to end the event, avert further listening
          }
        }
      }
      if (!handled) {
        std::string evdesc;
        if (eev.events & EPOLLIN) {
          evdesc += "EPOLLIN|";
        }
        if (eev.events & EPOLLOUT) {
          evdesc += "EPOLLOUT|";
        }
        if (eev.events & EPOLLERR) {
          evdesc += "EPOLLERR|";
        }
        if (eev.events & EPOLLHUP) {
          evdesc += "EPOLLHUP|";
        }
        if (eev.events & EPOLLPRI) {
          evdesc += "EPOLLPRI|";
        }
        if (!evdesc.empty()) evdesc.erase(evdesc.end()-1, evdesc.end());
        Logger::fuss("Unhandled (%s) event for fd=%d, events=0x%x", evdesc.c_str(), eev.data.fd, eev.events);

        if (eev.events & EPOLLHUP || eev.events & EPOLLERR) {
          Logger::fuss("Stopping listeners for fd=%d", eev.data.fd);
          stop_listen(eev.data.fd);
        }
      }
      mut.unlock();
    }

    if (r == -1) {
      if (error_handler) {
        error_handler(errno);
      } else {
        default_error_handler(errno);
      }
    }
  }
}

void EPollDispatcher::default_error_handler(int errcode)
{
  if (errcode == EINTR) {
    Logger::fuss("EPollDispatcher received signal during epoll_wait");
  } else if (errcode == EINVAL) {
    Logger::critical("EPollDispatcher: epoll_wait called on an object that is not an epoll. This is surely wrong so the thread is exiting.");
    running = false;
  } else if (errcode == EFAULT) {
    Logger::critical("EPollDispatcher: events field was not accessible with write permissions. The epoll_event should be created on the stack and so this shouldn't occur. Exiting thread.");
    running = false;
  } else if (errcode == EBADF) {
    Logger::error("Bad file descriptor passed to epoll_wait(), exiting thread.");
    running = false;
  } else {
    Logger::critical("Unidentifiable error returned by epoll_wait[%d], exiting thread.", errcode);
    running = false;
  }
}


void EPollDispatcher::stop_listen (int fd)
{
  std::lock_guard lock(mut);
  int r = 0;
  if (auto it = eventMap.find(fd); it != eventMap.end()) {
    r = epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, nullptr);
    if (r == -1) {
      Logger::warning("Couldn't delete events from an epoll (fd=%02d: %s)", fd, strerror(errno));
    }
    eventMap.erase(fd);
  }
  auto lit = listeners.bucket(fd);
  for (auto it = listeners.begin(lit); it != listeners.end(lit); it++) {
    delete it->second;
  }
  listeners.erase(fd);
}


void EPollDispatcher::add_listener (EPollListener* listener)
{
  std::lock_guard lock(mut);
  bool is_new = eventMap.find(*listener->pfd) == eventMap.end();
  listeners.insert({listener->pfd->fd, listener});
  if (is_new) {
    struct epoll_event mod_events;
    mod_events.events = listener->events & ~EPOLLOUT; // when adding a new EPOLLOUT listener, don't enable the event straightaway
    mod_events.data.u64 = 0UL;
    mod_events.data.fd = listener->pfd->fd;
    eventMap.insert({listener->pfd->fd, listener->events});
    int r = epoll_ctl(_epoll, EPOLL_CTL_ADD, listener->pfd->fd, &mod_events);
    assert_re(r != -1, "epoll_ctl with EPOLL_CTL_ADD failed. [\"%s\"]", strerror(errno));
  } else {
    modify(listener->pfd->fd, listener->events, 0);
  }
}


void EPollDispatcher::modify (int fd, unsigned int events_to_add, unsigned int events_to_remove)
{
  std::lock_guard lock(mut);
  eventMap[fd] |= events_to_add;
  eventMap[fd] &= ~events_to_remove;
  struct epoll_event mod_events;
  mod_events.data.u64 = 0UL;
  mod_events.data.fd = fd;
  mod_events.events = eventMap[fd];
  int r = epoll_ctl(_epoll, EPOLL_CTL_MOD, fd, &mod_events);
  assert_re(r != -1, "epoll_ctl with EPOLL_CTL_MOD failed. strerror(errno) = \"%s\"", strerror(errno));
  if (mod_events.events == 0) {
    eventMap.erase(fd);
  }
}


void EPollDispatcher::initialize ()
{
  if (!appioDispatch) {
    appioDispatch = std::make_shared<EPollDispatcher>();
    appioDispatch->start();
  }
}


void EPollDispatcher::finalize ()
{
  appioDispatch.reset();
}


std::shared_ptr<EPollDispatcher> EPollDispatcher::def()
{
  if (!static_cast<bool>(appioDispatch)) {
    throw std::logic_error("Default EPollDispatch is null");
  }
  return appioDispatch;
}
