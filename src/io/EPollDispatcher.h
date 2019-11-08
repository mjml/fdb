#ifndef EPOLLDISPATCHER_HPP
#define EPOLLDISPATCHER_HPP

#include <unordered_map>
#include <map>
#include <functional>
#include <thread>
#include <memory>
#include <sys/ioctl.h>
#include <sys/epoll.h>


struct TextEvent
{
public:

  template<typename S>
  TextEvent(int _type, S _text) : type(_type), text(std::forward<S>(_text)) { }

  ~TextEvent () = default;

  int type;
  std::string text;

};


struct ListenerDetails
{
  struct Compare
  {
    bool operator()(int lhs, int rhs) { return lhs < rhs; }
  };

  /**
   * The handler accepts epoll events and gives back two masks.
   * The first is a mask of events to ADD into the epoll for the given fd.
   * The second is a mask of events to REMOVE into the epoll for the given fd.
   *
   * So if you're handling an EPOLLOUT event and wrote a complete buffer, you can set
   * the second event field to EPOLLOUT and that event will subsequently be removed
   * from the epoll for that fd.
   *
   * @brief event_field_t
   */
  typedef decltype(epoll_event::events) event_field_t;
  typedef struct {
    bool handled = false;
    event_field_t add = 0;
    event_field_t rem = 0;
  } handler_ret_t ;

  typedef handler_ret_t func_t(const struct epoll_event& eev);
  typedef std::function<func_t> handler_t;

  int fd;
  uint32_t events;
  handler_t handler;

  ListenerDetails(int _fd, uint32_t _events, std::function<func_t>&& f) : fd(_fd), events(_events), handler(f) {}
  ListenerDetails(ListenerDetails&& x) : fd(x.fd), events(x.events), handler(std::move(x.handler)) {}

};

class EPollDispatcher
{
public:
  typedef std::function<void(int)> error_handler_t;

private:
  int _epoll;
  std::thread* th;
  bool running;
  std::unordered_multimap<int, ListenerDetails> listeners;
  std::map<int,decltype(epoll_event::events)> eventMap; // have to maintain userspace state for epoll fd's events field .. argh!
  error_handler_t error_handler;

public:
  EPollDispatcher () : _epoll(0), th(nullptr), running(false), eventMap(), error_handler() {}
  ~EPollDispatcher();

  void start();
  void join();

  template<typename F>
  void start_listen (int fd, uint32_t e, F handler) {
    ListenerDetails ld(fd, e, ListenerDetails::handler_t(handler));
    add_listener(std::move(ld));
  }

  void stop_listen (int fd);

  void add_listener(ListenerDetails&& ld);
  void modify (int fd, unsigned int events_to_add, unsigned int events_to_remove);

  static void initialize ();
  static void finalize ();
  static std::shared_ptr<EPollDispatcher> def();

protected:
  void run ();

  // default instance, though not the only possible instance
  static std::shared_ptr<EPollDispatcher> appioDispatch;

};


#endif // EPOLLDISPATCHER_HPP
