#ifndef EPOLLDISPATCHER_HPP
#define EPOLLDISPATCHER_HPP

#include <unordered_map>
#include <map>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/socket.h>


struct autoclosing_fd {
	int fd;
	autoclosing_fd(int fdesc) : fd(fdesc) {
		//Logger::debug2("autoclosing_fd constructed with .fd=%d, fdesc=%d", fd, fdesc);
	}
	~autoclosing_fd() {
		//Logger::debug2("autoclosing_fd destroyed with .fd=%d", fd);
		if (fd) {
			::close(fd);
		}
	}
	const autoclosing_fd& operator= (int fdesc) {
		//Logger::debug2("assigning autoclosing_fd from int (not from autoclosing_fd&&)");
		fd = fdesc;
		return *this;
	}
	const autoclosing_fd& operator= (autoclosing_fd&& moved) {
		fd = moved.fd;
		moved.fd = 0;
		return *this;
	}
	const autoclosing_fd& operator= (const autoclosing_fd& illegal) = delete;
	const autoclosing_fd& operator= (autoclosing_fd& illegal) = delete;
	operator int() { return fd; }
};

struct EPollListener
{
  struct Compare
  {
    bool operator()(int lhs, int rhs) { return lhs < rhs; }
  };

  /**
   * The handler accepts epoll events and gives back a flag and two masks.
   * The flag indicates whether or not the event was "consumed" with a corresponding read(), write(), accept().
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
  }                                  ret_t;

  typedef ret_t                      func_t(const struct epoll_event& eev);
  typedef std::function<func_t>      handler_t;
  typedef std::shared_ptr<autoclosing_fd>    pfd_t;

  pfd_t    pfd;
  uint32_t events;
  handler_t handler;

  EPollListener(pfd_t _pfd, uint32_t _events, std::function<func_t>&& f) : pfd(_pfd), events(_events), handler(f) {}
  EPollListener(EPollListener&& x) : pfd(std::move(x.pfd)), events(x.events), handler(std::move(x.handler)) {}
  virtual ~EPollListener();

};


struct SingleClientAcceptor : public EPollListener
{
  /* The user's handler, which will be installed once the server accepts a single client and has closed the original server socket. */
  handler_t deferredHandler;

  template<typename F>
  SingleClientAcceptor (EPollListener::pfd_t serverSocket, F&& handler) :
    EPollListener(serverSocket, EPOLLIN, std::bind(&SingleClientAcceptor::acceptFunction, this, std::placeholders::_1)),
    deferredHandler(handler) {}

  ~SingleClientAcceptor() = default;

  ret_t acceptFunction (const struct epoll_event& eev);

};


class EPollDispatcher
{
public:
  typedef std::function<void(int)> error_handler_t;

private:
  int		           _epoll;
  std::thread*         th;
  bool                 running;
  std::unordered_multimap<int, EPollListener*>    listeners;
  std::map<int,decltype(epoll_event::events)>    eventMap;
  error_handler_t	   error_handler;

public:
  std::recursive_mutex mut;

public:
  EPollDispatcher () : _epoll(0), th(nullptr), running(false), eventMap(), error_handler() {}
  ~EPollDispatcher();

  void start();
  void join();

  /**
   * Call this on a server socket fd. (One that's been bound with bind()). It will temporarily install an accept() handler,
   *   so that when an incoming connection request is made, it will automatically accept() and close() the original socket,
   *   then replace that socket's fd with the newly formed client socket's fd.
   * The new client socket will be registered with the epoll using only EPOLLIN.
   */
  template<typename F>
  void start_singleclient_listener (EPollListener::pfd_t pfd, F handler) {
	  auto acceptor = new SingleClientAcceptor (pfd, handler);
	  add_listener(acceptor);
	  ::listen(*pfd, 1);
  }

  /**
   * Call this to be notified of generic epoll_events.
   */
  template<typename F>
  void start_listen (EPollListener::pfd_t pfd, uint32_t event_mask, F handler) {
     auto ld = new EPollListener(pfd, event_mask, EPollListener::handler_t(handler));
	 add_listener(ld);
  }

  void stop_listen (int fd);

  void add_listener (EPollListener* ld);

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
