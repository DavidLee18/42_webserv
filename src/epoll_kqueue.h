//
// Created by 이재현 on 2025-11-12.
//

#ifndef INC_42_WEBSERV_EPOLL_KQUEUE_H
#define INC_42_WEBSERV_EPOLL_KQUEUE_H

#include "file_descriptor.h"
#include "vec.h"

#ifdef __APPLE__
#include <iterator>
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
  friend const FileDescriptor &
  add_event(FileDescriptor, KQueue &,
            const Event &) throw(InvalidFileDescriptorException);
  friend void del_event(const FileDescriptor &, KQueue &,
                        const Event &) throw(InvalidFileDescriptorException);
};
#else

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
  Event(const FileDescriptor &fd, const bool in, const bool out,
        const bool rdhup, const bool pri, const bool err, const bool hup)
      : fd(fd), in(in), out(out), rdhup(rdhup), pri(pri), err(err), hup(hup) {}
  const FileDescriptor &fd;
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
  const Event *_Nonnull _events;

public:
  Events(const Vec<FileDescriptor> &, size_t,
         const epoll_event *_Nonnull) throw(FdNotRegisteredException);
  ~Events();
  bool is_end() const;
  Events &operator++() throw(IteratorEndedException);
  const Event &operator*() const throw(IteratorEndedException);
};

/**
 * @class EPoll
 * @brief A simple epoll wrapper class.
 */
class EPoll {
  FileDescriptor _fd;
  Vec<FileDescriptor> _events;
  size_t _size;

public:
  EPoll(size_t) throw(InvalidFileDescriptorException);
  Events wait(const int timeout_ms) throw(InterruptedException);
  friend const FileDescriptor &
  add_fd(FileDescriptor, EPoll &, const Event &,
         const Option &) throw(InvalidOperationException, EPollLoopException,
                               FdNotRegisteredException, OutOfMemoryException,
                               EPollFullException,
                               NotSupportedOperationException);
  friend void modify_fd(const FileDescriptor &, EPoll &, const Event &ev,
                        const Option &op) throw(InvalidOperationException,
                                                FdNotRegisteredException,
                                                OutOfMemoryException,
                                                NotSupportedOperationException);
  friend void del_fd(const FileDescriptor &fd,
                     EPoll &ep) throw(InvalidOperationException,
                                      FdNotRegisteredException,
                                      OutOfMemoryException,
                                      NotSupportedOperationException);
};

#endif

#endif // INC_42_WEBSERV_EPOLL_KQUEUE_H
