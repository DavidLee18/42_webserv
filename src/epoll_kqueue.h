#ifndef EPOLL_KQUEUE_H
#define EPOLL_KQUEUE_H

#include "file_descriptor.h"
#include <cstddef>
#include <iterator>
#include <vector>

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>

/**
 * @class Event
 * @brief Represents an event with associated file descriptor and operation
 * flags.
 *
 * Event is used to describe the state and operations associated with a file
 * descriptor in an I/O event-driven context. It includes flags indicating
 * whether the event is for reading, writing, or deletion.
 */
class Event {
public:
  const FileDescriptor &fd;
  const bool read;
  const bool write;
  const bool delete_;
  Event(const FileDescriptor &fd, const bool read, const bool write,
        const bool is_delete)
      : fd(fd), read(read), write(write), delete_(is_delete) {}
  Event(const Event &other)
      : fd(other.fd), read(other.read), write(other.write), delete_(other.delete_) {}
private:
  Event& operator=(const Event &); // Immutable - no assignment
};

/**
 * @class Events
 * @brief An iterator class to handle and represent a collection of events.
 *
 * The Events class provides functionality to store, manage, and manipulate
 * a set of events. It offers operation to query events, ensuring
 * efficient and structured event management within an application.
 */
class Events : public std::iterator<std::input_iterator_tag, Event, long,
                                    const Event *, const Event &> {
  size_t _curr;
  Event *_Nonnull _events;
  size_t _len;

public:
  explicit Events(size_t);
  ~Events();
  Events end() const;
  Events &operator++() throw(IteratorEndedException);
  Events operator++(int) throw(IteratorEndedException);
  bool operator==(const Events &) const;
  bool operator!=(const Events &) const;
  const Event &operator*() const;
};

/**
 * @class KQueue
 * @brief A class for managing and interacting with kernel event queues.
 *
 * The KQueue class encapsulates functionalities for creating and managing
 * kernel queues, primarily used for monitoring and handling I/O events
 * and other event-driven scenarios. It facilitates efficient event
 * notification mechanisms within applications.
 */
class KQueue {
  size_t _cap;
  FileDescriptor _fd;
  Vec<FileDescriptor> _events;

public:
  explicit KQueue(size_t);
  ~KQueue();
  Events wait(int timeout_ms);
  const FileDescriptor &
  add_event(FileDescriptor,
            const Event &) throw(InvalidFileDescriptorException);
  void del_event(const FileDescriptor &,
                 const Event &) throw(InvalidFileDescriptorException);
};
#else // __APPLE__

#include <sys/epoll.h>

/**
 * @class Option
 * @brief A class representing configuration options for epoll events.
 *
 * Option encapsulates a set of flags that define the behavior of epoll
 * operations, such as edge-triggered mode, one-shot behavior, wake-up events,
 * and exclusive event handling.
 * 
 * IMPORTANT: When using edge-triggered mode (et=true), file descriptors MUST be
 * set to non-blocking mode using FileDescriptor::set_nonblocking(). This is
 * because edge-triggered mode only notifies once per state change, requiring
 * the application to drain all available data in a loop. Without non-blocking
 * mode, operations could block indefinitely, freezing the event loop.
 */
class Option {
public:
  Option(const bool et, const bool oneshot, const bool wakeup,
         const bool exclusive)
      : et(et), oneshot(oneshot), wakeup(wakeup), exclusive(exclusive) {}
  Option(const Option &other)
      : et(other.et), oneshot(other.oneshot), wakeup(other.wakeup), 
        exclusive(other.exclusive) {}
  const bool et;
  const bool oneshot;
  const bool wakeup;
  const bool exclusive;
private:
  Option& operator=(const Option &); // Immutable - no assignment
};

/**
 * @class Event
 * @brief A class to encapsulate event data for epoll-based operations.
 *
 * The Event class represents an individual event associated with a file
 * descriptor, capturing various event flags such as readability, writability,
 * and error states. It is used for monitoring the state and activity of file
 * descriptors in the context of an epoll event loop.
 */
class Event {
public:
  Event(const FileDescriptor *fd, const bool in, const bool out,
        const bool rdhup, const bool pri, const bool err, const bool hup)
      : fd(fd), in(in), out(out), rdhup(rdhup), pri(pri), err(err), hup(hup) {}
  Event(const Event &other)
      : fd(other.fd), in(other.in), out(other.out), rdhup(other.rdhup),
        pri(other.pri), err(other.err), hup(other.hup) {}
  const FileDescriptor *fd;
  const bool in;
  const bool out;
  const bool rdhup;
  const bool pri;
  const bool err;
  const bool hup;
private:
  Event& operator=(const Event &); // Immutable - no assignment
};

/**
 * @class Events
 * @brief An input iterator class for traversing epoll events.
 *
 * The Events class provides an interface to iterate over a collection of
 * epoll events. It encapsulates event data and supports standard input iterator
 * operations such as increment, equality comparison, and dereferencing.
 *
 * This class is specifically designed to work with epoll-based event
 * collections, facilitating efficient iteration over the events with proper
 * handling of underlying resources.
 */
class Events : public std::iterator<std::input_iterator_tag, Event, long,
                                    const Event *, const Event &> {
  size_t _curr;
  size_t _len;
  Event *_events;

  Events() : _curr(0), _len(0), _events(NULL) {}

public:
  // Move-like copy constructor: transfers ownership from other (for Result pattern)
  // Note: Uses const_cast to enable move semantics in C++98
  Events(const Events &other) : _curr(other._curr), _len(other._len), _events(other._events) {
    // Invalidate other to prevent double-free (cast away const for move semantics)
    Events &mutable_other = const_cast<Events&>(other);
    mutable_other._curr = 0;
    mutable_other._len = 0;
    mutable_other._events = NULL;
  }

  ~Events();
  static Result<Events> init(const std::vector<FileDescriptor> &, size_t,
                             const epoll_event *);
  bool is_end() const;
  Result<Void> operator++();
  Result<const Event *> operator*() const;

private:
  // No assignment operator - Events is not assignable
  Events& operator=(const Events &);
};

/**
 * @class EPoll
 * @brief A simple epoll wrapper class.
 * 
 * This class wraps the Linux epoll API for scalable I/O event notification.
 * When using edge-triggered mode (EPOLLET), file descriptors MUST be set to
 * non-blocking mode. See Option class documentation for details.
 * 
 * Typical usage pattern:
 * 1. Create EPoll instance with EPoll::create()
 * 2. Create socket and call FileDescriptor::set_nonblocking()
 * 3. Add socket to EPoll with add_fd() using edge-triggered Option
 * 4. In event loop, drain all data with while(!EWOULDBLOCK) pattern
 */
class EPoll {
  FileDescriptor _fd;
  std::vector<FileDescriptor> _events;
  unsigned short _size;
  
public:
  EPoll() : _fd(), _events(), _size(0) {}
  
  // Move-like copy constructor: transfers ownership from other, leaving it empty
  // Note: Uses const_cast to enable move semantics in C++98
  EPoll(const EPoll &other) : _fd(other._fd), _size(other._size) {
    // Move the events vector instead of copying to avoid invalidating FileDescriptors
    EPoll &mutable_other = const_cast<EPoll&>(other);
    _events.swap(mutable_other._events);
    // Invalidate other - make it empty
    mutable_other._size = 0;
    // FileDescriptor will handle its own state
  }
  
  // Move-like assignment operator: transfers ownership from other, leaving it empty
  // Note: Uses const_cast to enable move semantics in C++98
  EPoll& operator=(const EPoll &other) {
    if (this != &other) {
      // Transfer resources using swap to avoid copying FileDescriptors
      _fd = other._fd;
      _size = other._size;
      EPoll &mutable_other = const_cast<EPoll&>(other);
      _events.swap(mutable_other._events);
      // Invalidate other - make it empty
      mutable_other._size = 0;
    }
    return *this;
  }
  
  static Result<EPoll> create(unsigned short);
  Result<Events> wait(const int timeout_ms);
  Result<const FileDescriptor *> add_fd(FileDescriptor, const Event &,
                                        const Option &);
  Result<Void> modify_fd(FileDescriptor &, const Event &, const Option &);
  Result<Void> del_fd(FileDescriptor &);
};

#endif // __APPLE__

#endif // EPOLL_KQUEUE_H
