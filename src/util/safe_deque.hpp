#ifndef QSAFEDEQUE_H
#define QSAFEDEQUE_H

#include <atomic>
#include <list>
#include <mutex>

template<typename T>
class safe_deque
{
public:
  explicit safe_deque();
  ~safe_deque();

public:
  // push by construction
  template<typename...Args>
  void safe_emplace_front(Args...);

  // push by reference
  void push_front(T* t);
  void safe_push_front(T* t);

  // pop pointer, caller deletes
  T* pop_back();
  T* safe_pop_back();

  int size();
  int safe_size();

  bool empty();
  bool safe_empty();

  void fast_lock ();
  void fast_unlock ();

protected:
  std::list<T*>    lst;
  std::atomic_flag chk;

};

template<typename T>
inline safe_deque<T>::safe_deque() : lst(), chk(false)
{
}

template<typename T>
inline safe_deque<T>::~safe_deque()
{
  fast_lock();
  while (!lst.empty()) {
    T* item = pop_back();
    delete item;
  }
  fast_unlock();
}

template<typename T>
template<typename...Args>
inline void safe_deque<T>::safe_emplace_front(Args...args)
{
  T* newObj = new T(args...);
  safe_push_front(newObj);
}

template<typename T>
inline void safe_deque<T>::push_front(T* t)
{
  lst.push_front(t);
}

template<typename T>
inline void safe_deque<T>::safe_push_front(T* t)
{
  fast_lock();
  push_front(t);
  fast_unlock();
}


template<typename T>
inline T* safe_deque<T>::pop_back()
{
  T* item = lst.back();
  lst.pop_back();
  return item;
}

template<typename T>
inline T* safe_deque<T>::safe_pop_back()
{
  fast_lock();
  T* item = pop_back();
  fast_unlock();
  return item;
}

template<typename T>
inline int safe_deque<T>::size()
{
  return lst.size();
}


template<typename T>
inline int safe_deque<T>::safe_size()
{
  int sz = 0;
  fast_lock();
  sz = size();
  fast_unlock();
  return sz;
}

template<typename T>
inline bool safe_deque<T>::empty()
{
  return lst.empty();
}

template<typename T>
inline bool safe_deque<T>::safe_empty()
{
  bool r = false;
  fast_lock();
  r = empty();
  fast_unlock();
  return r;
}

template<typename T>
void safe_deque<T>::fast_lock ()
{
  bool prev = chk.test_and_set();
  while (prev) {
    pthread_yield();
    prev = chk.test_and_set();
  }
}

template<typename T>
void safe_deque<T>::fast_unlock ()
{
  chk.clear();
}


#endif // QSAFEDEQUE_H
