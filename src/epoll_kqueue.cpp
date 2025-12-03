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
Result<Events> Events::init(const Vec<FileDescriptor> &all_events, size_t size,
                            const epoll_event *events) {
  Events *es = new Events();
  es->_events = static_cast<Event *>(operator new(sizeof(Event) * size));
  FileDescriptor *fd = NULL;
  for (size_t i = 0; i < size; i++) {
    TRY(Events, fd, all_events.find(events[i].data.fd))
    new ((void *)(es->_events + i)) Event(fd, (events[i].events & EPOLLIN) == 1,
                                          (events[i].events & EPOLLOUT) == 1,
                                          (events[i].events & EPOLLRDHUP) == 1,
                                          (events[i].events & EPOLLPRI) == 1,
                                          (events[i].events & EPOLLERR) == 1,
                                          (events[i].events & EPOLLHUP) == 1);
  }
  delete[] events;
  return OK(Events, es);
}

Events::~Events() {
  // for (size_t i = 0; i < _len; i++)
  // _events[i].~Event();
  operator delete((void *)_events);
}

bool Events::is_end() const { return _curr >= _len; }

Result<Void> Events::operator++() {
  if (_curr >= _len)
    return ERR(Void, errors::iter_ended);
  ++_curr;
  return OK(Void, new Void);
}

const Result<Event> Events::operator*() const {
  if (_curr >= _len) {
    return ERR(Event, errors::iter_ended);
  }
  return OK(Event, _events + _curr);
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
