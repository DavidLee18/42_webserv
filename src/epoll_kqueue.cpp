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

Result<EPoll> EPoll::create(unsigned short sz) {
  int fd = epoll_create(static_cast<int>(sz));
  if (fd < 0) {
    switch (fd) {
    case EINVAL:
      return ERR(EPoll, "the epoll size is zero");
    case EMFILE:
    case ENFILE:
      return ERR(EPoll, errors::fd_too_many);
    case ENOMEM:
      return ERR(EPoll, errors::out_of_mem);
    default:
      return ERR(EPoll, "an unknown epoll error occured");
    }
  }
  EPoll *ep = new EPoll();
  ep->_size = sz;
  TRY(EPoll, ep->_fd, FileDescriptor::from_raw(fd))
  return OK(EPoll, ep);
}

const Result<FileDescriptor> EPoll::add_fd(FileDescriptor fd, const Event &ev,
                                           const Option &op) {
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
  event.data.fd = fd._fd;
  if (epoll_ctl(_fd->_fd, EPOLL_CTL_ADD, fd._fd, &event) == -1) {
    switch (errno) {
    case EEXIST:
      return ERR(FileDescriptor, "this fd is already registered to this epoll");
    case EINVAL:
      return ERR(FileDescriptor, errors::invalid_fd);
    case ELOOP:
      return ERR(FileDescriptor, errors::epoll_loop);
    case ENOMEM:
      return ERR(FileDescriptor, errors::out_of_mem);
    case ENOSPC:
      return ERR(FileDescriptor, errors::epoll_full);
    case EPERM:
      return ERR(FileDescriptor, errors::not_supported);
    default:
      return ERR(FileDescriptor,
                 "an unknown error occured during EPOLL_CTL_ADD");
    }
  }
  _events.push(fd);
  FileDescriptor *fd_in;
  TRY(FileDescriptor, fd_in, _events.find(fd))
  return OK(FileDescriptor, fd_in);
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
