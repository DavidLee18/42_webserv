//
// Created by 이재현 on 2025-11-10.
//
#include "webserv.h"

#ifdef __APPLE__
const FileDescriptor &
KQueue::add_event(FileDescriptor fd,
                  const Event &e) throw(InvalidFileDescriptorException) {
  if (fd != ev.fd)
    throw InvalidFileDescriptorException();
  struct kevent event = {};
  if (ev.read)
    if (kevent(*kq._fd, &event, 1, NULL, 0, NULL) == -1)
      throw std::runtime_error("kevent failed");
  kq._events.push(fd);
  return kq._events[kq._events.size() - 1];
}

void KQueue::del_event(const FileDescriptor &fd,
                       const Event &e) throw(InvalidFileDescriptorException) {
  /* TODO */
}
#else
Events::Events(const Vec<FileDescriptor> &all_events, const size_t size,
               const epoll_event *events) throw(FdNotRegisteredException)
    : _curr(0), _len(size),
      _events(static_cast<Event *>(operator new(sizeof(Event) * size))) {
  try {
    for (size_t i = 0; i < size; i++) {
      new ((void *)(_events + i)) Event(all_events.find(events[i].data.fd),
                                        (events[i].events & EPOLLIN) == 1,
                                        (events[i].events & EPOLLOUT) == 1,
                                        (events[i].events & EPOLLRDHUP) == 1,
                                        (events[i].events & EPOLLPRI) == 1,
                                        (events[i].events & EPOLLERR) == 1,
                                        (events[i].events & EPOLLHUP) == 1);
    }
    delete[] events;
  } catch (std::exception) {
    delete[] events;
    throw FdNotRegisteredException();
  }
}

Events::~Events() {
  for (size_t i = 0; i < _len; i++)
    _events[i].~Event();
  operator delete((void *)_events);
}

bool Events::is_end() const { return _curr >= _len; }

Events &Events::operator++() throw(IteratorEndedException) {
  if (_curr >= _len) {
    throw IteratorEndedException();
  }
  ++_curr;
  return *this;
}

const Event &Events::operator*() const throw(IteratorEndedException) {
  if (_curr >= _len) {
    throw IteratorEndedException();
  }
  return _events[_curr];
}

EPoll::EPoll(size_t size) throw(InvalidFileDescriptorException)
    : _fd(epoll_create(size)), _events(), _size(size) {
  if (*_fd == -1) {
    throw InvalidFileDescriptorException();
  }
}

const FileDescriptor &
EPoll::add_fd(FileDescriptor fd, const Event &ev, const Option &op) throw(
    InvalidOperationException, EPollLoopException, FdNotRegisteredException,
    OutOfMemoryException, EPollFullException, NotSupportedOperationException) {
  epoll_event event = {};
  if (ev.in)
    event.events |= EPOLLIN;
  if (ev.out)
    event.events |= EPOLLOUT;
  if (ev.rdhup)
    event.events |= EPOLLRDHUP;
  if (ev.pri)
    event.events |= EPOLLPRI;
  if (ev.err)
    event.events |= EPOLLERR;
  if (ev.hup)
    event.events |= EPOLLHUP;
  if (op.et)
    event.events |= EPOLLET;
  if (op.oneshot)
    event.events |= EPOLLONESHOT;
  if (op.wakeup)
    event.events |= EPOLLWAKEUP;
  if (op.exclusive)
    event.events |= EPOLLEXCLUSIVE;
  event.data.fd = *fd;
  if (epoll_ctl(*_fd, EPOLL_CTL_ADD, *fd, &event) == -1) {
    switch (errno) {
    case EEXIST:
      throw InvalidOperationException();
    case EINVAL:
      throw InvalidOperationException();
    case ELOOP:
      throw EPollLoopException();
    case ENOENT:
      throw FdNotRegisteredException();
    case ENOMEM:
      throw OutOfMemoryException();
    case ENOSPC:
      throw EPollFullException();
    case EPERM:
      throw NotSupportedOperationException();
    }
  }
  _events.push(fd);
  return _events[_events.size() - 1];
}

void EPoll::modify_fd(const FileDescriptor &fd, const Event &ev,
                      const Option &op) throw(InvalidOperationException,
                                              FdNotRegisteredException,
                                              OutOfMemoryException,
                                              NotSupportedOperationException) {
  epoll_event event = {};
  if (ev.in)
    event.events |= EPOLLIN;
  if (ev.out)
    event.events |= EPOLLOUT;
  if (ev.rdhup)
    event.events |= EPOLLRDHUP;
  if (ev.pri)
    event.events |= EPOLLPRI;
  if (ev.err)
    event.events |= EPOLLERR;
  if (ev.hup)
    event.events |= EPOLLHUP;
  if (op.et)
    event.events |= EPOLLET;
  if (op.oneshot)
    event.events |= EPOLLONESHOT;
  if (op.wakeup)
    event.events |= EPOLLWAKEUP;
  if (op.exclusive)
    event.events |= EPOLLEXCLUSIVE;
  event.data.fd = *fd;
  if (epoll_ctl(*_fd, EPOLL_CTL_MOD, *fd, &event) == -1) {
    switch (errno) {
    case EINVAL:
      throw InvalidOperationException();
    case ENOENT:
      throw FdNotRegisteredException();
    case ENOMEM:
      throw OutOfMemoryException();
    case EPERM:
      throw NotSupportedOperationException();
    }
  }
}

void EPoll::del_fd(const FileDescriptor &fd) throw(
    InvalidOperationException, FdNotRegisteredException, OutOfMemoryException,
    NotSupportedOperationException) {
  epoll_event event = {};
  event.data.fd = *fd;
  if (epoll_ctl(*_fd, EPOLL_CTL_DEL, *fd, &event) == -1) {
    switch (errno) {
    case EINVAL:
      throw InvalidOperationException();
    case ENOENT:
      throw FdNotRegisteredException();
    case ENOMEM:
      throw OutOfMemoryException();
    case EPERM:
      throw NotSupportedOperationException();
    }
  }
}

Events EPoll::wait(const int timeout_ms) throw(InterruptedException) {
  struct epoll_event *events = new struct epoll_event[_size];
  int n = epoll_wait(*_fd, events, _size, timeout_ms);
  if (n == -1) {
    throw InterruptedException();
  }
  return Events(_events, n, events);
}
#endif
