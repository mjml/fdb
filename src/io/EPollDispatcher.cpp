#include <sys/ioctl.h>
#include <sys/epoll.h>
#include "util/safe_deque.hpp"
#include "util/exceptions.hpp"
#include "EPollDispatcher.h"

std::shared_ptr<EPollDispatcher> EPollDispatcher::appioDispatch;

EPollDispatcher::~EPollDispatcher ()
{
  listeners.clear();
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
}


void EPollDispatcher::run ()
{
  int r = 0;
  struct epoll_event eev;

  while (running) {
    eev.data.u64 = 0;
    eev.events = 0;
    r = epoll_wait(_epoll, &eev, 1, 400);
    int fd = eev.data.fd;
    if (r > 0) {
      auto bkt = listeners.bucket(fd);
      for (auto it = listeners.begin(bkt); it != listeners.end(bkt); it++) {
        const ListenerDetails& ld = it->second;
        if (eev.events & ld.events) {
          auto p = ld.handler(eev);
          if (p.add|| p.rem) {
            modify(fd, p.add, p.rem);
            if (p.handled) break; // if the listener signals to end the event, avert further listening
          }
        }
      }
    }
    if (r == -1) {
      if (error_handler) {
        error_handler(errno);
      }
    }
  }
}


void EPollDispatcher::stop_listen (int fd)
{
  int r = 0;
  if (eventMap.find(fd) != eventMap.end()) {
    r = epoll_ctl(_epoll, EPOLL_CTL_DEL, fd, nullptr);
    if (r == -1) {
      Logger::warning("Couldn't delete events from an epoll (fd=%02d: %s)", fd, strerror(errno));
    }
    eventMap.erase(fd);
  }
  if (auto lit = listeners.find(fd); lit != listeners.end()) {
    listeners.erase(lit);
  }

}


void EPollDispatcher::add_listener (ListenerDetails&& ld)
{
  bool is_new = eventMap.find(ld.fd) == eventMap.end();
  listeners.emplace(ld.fd, std::move(ld));
  if (is_new) {
    struct epoll_event mod_events;
    mod_events.events = ld.events & ~EPOLLOUT; // when adding a new EPOLLOUT listener, don't enable the event straightaway
    mod_events.data.u64 = 0UL;
    mod_events.data.fd = ld.fd;
    eventMap.insert({ld.fd, ld.events});
    int r = epoll_ctl(_epoll, EPOLL_CTL_ADD, ld.fd, &mod_events);
    assert_re(r != -1, "epoll_ctl with EPOLL_CTL_ADD failed. strerror(errno) = \"%s\"");
  } else {
    modify(ld.fd, ld.events, 0);
  }
}


void EPollDispatcher::modify (int fd, unsigned int events_to_add, unsigned int events_to_remove)
{
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
