#ifndef EPOLL_HPP
#define EPOLL_HPP

#include "FileDescriptor.hpp"
#include <cstddef>
#include <iterator>
#include <list>

#include <sys/epoll.h>

// A class representing configuration options for epoll events.
class Option {
  public:
    Option(const bool et, const bool oneshot, const bool wakeup,
           const bool exclusive)
        : et(et), oneshot(oneshot), wakeup(wakeup), exclusive(exclusive) {}
    Option(const Option& other)
        : et(other.et), oneshot(other.oneshot), wakeup(other.wakeup),
          exclusive(other.exclusive) {}
    const bool et;
    const bool oneshot;
    const bool wakeup;
    const bool exclusive;

  private:
    Option& operator=(const Option&); // Immutable - no assignment
};

// A class to encapsulate event data for epoll-based operations.
class Event {
  public:
    Event(const FileDescriptor* fd, const bool in, const bool out,
          const bool rdhup, const bool pri, const bool err, const bool hup)
        : fd(fd), in(in), out(out), rdhup(rdhup), pri(pri), err(err), hup(hup) {
    }
    Event(const Event& other)
        : fd(other.fd), in(other.in), out(other.out), rdhup(other.rdhup),
          pri(other.pri), err(other.err), hup(other.hup) {}
    const FileDescriptor* fd;
    const bool            in;
    const bool            out;
    const bool            rdhup;
    const bool            pri;
    const bool            err;
    const bool            hup;

  private:
    Event& operator=(const Event&); // Immutable - no assignment
};

// An input iterator class for traversing epoll events.
class Events : public std::iterator<std::input_iterator_tag, Event, long,
                                    const Event*, const Event&> {
    size_t _curr;
    size_t _len;
    Event* _events;

    Events() : _curr(0), _len(0), _events(NULL) {}

  public:
    // Move-like copy constructor: transfers ownership from other (for Result
    // pattern) Note: Uses const_cast to enable move semantics in C++98
    Events(const Events& other)
        : _curr(other._curr), _len(other._len), _events(other._events) {
        // Invalidate other to prevent double-free (cast away const for move
        // semantics)
        Events& mutable_other = const_cast<Events&>(other);
        mutable_other._curr   = 0;
        mutable_other._len    = 0;
        mutable_other._events = NULL;
    }

    ~Events();
    static Result<Events> init(const std::list<FileDescriptor>&, size_t,
                               const epoll_event*);
    bool                  is_end() const;
    Result<Void>          operator++();
    Result<const Event*>  operator*() const;

  private:
    // No assignment operator - Events is not assignable
    Events& operator=(const Events&);
};

// A simple epoll wrapper class.
class EPoll {
    FileDescriptor            _fd;
    std::list<FileDescriptor> _events;
    unsigned short            _size;

  public:
    EPoll() : _fd(), _events(), _size(0) {}

    // Move-like copy constructor: transfers ownership from other, leaving it
    // empty Note: Uses const_cast to enable move semantics in C++98
    EPoll(const EPoll& other) : _fd(other._fd), _size(other._size) {
        // Move the events vector instead of copying to avoid invalidating
        // FileDescriptors
        EPoll& mutable_other = const_cast<EPoll&>(other);
        _events.swap(mutable_other._events);
        // Invalidate other - make it empty
        mutable_other._size = 0;
        // FileDescriptor will handle its own state
    }

    // Move-like assignment operator: transfers ownership from other, leaving it
    // empty Note: Uses const_cast to enable move semantics in C++98
    EPoll& operator=(const EPoll& other) {
        if (this != &other) {
            // Transfer resources using swap to avoid copying FileDescriptors
            _fd                  = other._fd;
            _size                = other._size;
            EPoll& mutable_other = const_cast<EPoll&>(other);
            _events.swap(mutable_other._events);
            // Invalidate other - make it empty
            mutable_other._size = 0;
        }
        return *this;
    }

    bool                    event_contains(const FileDescriptor* fd) const;
    static Result<EPoll>    create(unsigned short);
    Result<Events>          wait(int timeout_ms) const;
    Result<FileDescriptor*> add_fd(FileDescriptor fd, Event&, const Option&);
    Result<Void>            modify_fd(const FileDescriptor*, const Event&,
                                      const Option&) const;
    Result<Void>            del_fd(const FileDescriptor*);
};

#endif
