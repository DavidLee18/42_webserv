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
 */
class Option {
public:
  Option(const bool et, const bool oneshot, const bool wakeup,
         const bool exclusive)
      : et(et), oneshot(oneshot), wakeup(wakeup), exclusive(exclusive) {}
  const bool et;
  const bool oneshot;
  const bool wakeup;
  const bool exclusive;
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
  const FileDescriptor *fd;
  const bool in;
  const bool out;
  const bool rdhup;
  const bool pri;
  const bool err;
  const bool hup;
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
  ~Events();
  static Result<Events> init(const std::vector<FileDescriptor> &, size_t,
                             const epoll_event *);
  bool is_end() const;
  Result<Void> operator++();
  Result<const Event *> operator*() const;
};

/**
 * @class EPoll
 * @brief A simple epoll wrapper class.
 */
class EPoll {
  FileDescriptor *_fd;
  std::vector<FileDescriptor> _events;
  unsigned short _size;
  EPoll() : _fd(NULL), _events(), _size(0) {}
  Result<Void> init();

public:
  static Result<EPoll> create(unsigned short);
  Result<Events> wait(const int timeout_ms);
  Result<const FileDescriptor *> add_fd(FileDescriptor, const Event &,
                                        const Option &);
  Result<Void> modify_fd(const FileDescriptor &, const Event &, const Option &);
  Result<Void> del_fd(const FileDescriptor &);
};

#endif // __APPLE__

#endif // EPOLL_KQUEUE_H
