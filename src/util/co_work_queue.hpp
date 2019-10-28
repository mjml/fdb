#ifndef LITTLE_WORK_QUEUE_HPP
#define LITTLE_WORK_QUEUE_HPP

#include <list>
#include <functional>
#include <algorithm>
#include <boost/coroutine2/coroutine.hpp>


template<typename T>
struct co_work_queue
{
  typedef boost::coroutines2::coroutine<T> coro_t;

  co_work_queue() : workers() {}
  ~co_work_queue() = default;

  void push_worker (typename coro_t::push_type&& pusher)
  {
    workers.emplace_back(std::move(pusher));
  }

  void push_input (T arg)
  {
    if (workers.empty()) { return; }
    typename coro_t::push_type& rf = workers.front();
    rf(arg);
    if (!rf) {
      workers.pop_front();
    }
  }

  std::list<typename coro_t::push_type> workers;
};





#endif // LITTLE_WORK_QUEUE_HPP
