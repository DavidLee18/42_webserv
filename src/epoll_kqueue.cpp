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
#else // LINUX
Result<Events> Events::init(const std::vector<FileDescriptor> &all_events,
                            size_t size, const epoll_event *events) {
  Events *es = new Events();
  es->_events = static_cast<Event *>(operator new(sizeof(Event) * size));
  for (size_t i = 0; i < size; i++) {
    FileDescriptor const **fd = NULL;
    for (size_t j = 0; j < all_events.size(); i++) {
      if (events[i].data.fd == all_events.at(j)) {
        fd = new const FileDescriptor *;
        *fd = FileDescriptor::from_raw(events[i].data.fd).value();
      }
    }
    if (fd == NULL) {
      operator delete((void *)es->_events);
      delete es;
      return ERR(Events, Errors::not_found);
    }
    new ((void *)(es->_events + i)) Event(
        *fd, (events[i].events & EPOLLIN) == 1,
        (events[i].events & EPOLLOUT) == 1,
        (events[i].events & EPOLLRDHUP) == 1,
        (events[i].events & EPOLLPRI) == 1, (events[i].events & EPOLLERR) == 1,
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
    return ERR(Void, Errors::iter_ended);
  ++_curr;
  return OK(Void, new Void);
}

Result<const Event *> Events::operator*() const {
  if (_curr >= _len) {
    return ERR(const Event *, Errors::iter_ended);
  }
  const Event **evpp = new const Event *;
  *evpp = _events + _curr;
  return OK(const Event *, evpp);
}

Result<EPoll> EPoll::create(unsigned short sz) {
  int fd = epoll_create(static_cast<int>(sz));
  if (fd < 0) {
    switch (fd) {
    case EINVAL:
      return ERR(EPoll, "the epoll size is zero");
    case EMFILE:
    case ENFILE:
      return ERR(EPoll, Errors::fd_too_many);
    case ENOMEM:
      return ERR(EPoll, Errors::out_of_mem);
    default:
      return ERR(EPoll, "an unknown epoll error occured");
    }
  }
  EPoll *ep = new EPoll();
  ep->_size = sz;
  TRY(EPoll, FileDescriptor, ep->_fd, FileDescriptor::from_raw(fd))
  return OK(EPoll, ep);
}

Result<const FileDescriptor *> EPoll::add_fd(FileDescriptor fd, const Event &ev,
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
      return ERR(const FileDescriptor *,
                 "this fd is already registered to this epoll");
    case EINVAL:
      return ERR(const FileDescriptor *, Errors::invalid_fd);
    case ELOOP:
      return ERR(const FileDescriptor *, Errors::epoll_loop);
    case ENOMEM:
      return ERR(const FileDescriptor *, Errors::out_of_mem);
    case ENOSPC:
      return ERR(const FileDescriptor *, Errors::epoll_full);
    case EPERM:
      return ERR(const FileDescriptor *, Errors::not_supported);
    default:
      return ERR(const FileDescriptor *,
                 "an unknown error occured during EPOLL_CTL_ADD");
    }
  }
  _events.push_back(fd);
  FileDescriptor const **fd_in;

  for (size_t i = 0; i < _events.size(); i++) {
    if (fd == _events.at(i)) {
      fd_in = new const FileDescriptor *();
      *fd_in = FileDescriptor::move_from(fd).value();
      return OK(const FileDescriptor *, fd_in);
    }
  }
  return ERR(const FileDescriptor *, Errors::not_found);
}

Result<Void> EPoll::modify_fd(const FileDescriptor &fd, const Event &ev,
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
  if (epoll_ctl(_fd->_fd, EPOLL_CTL_MOD, fd._fd, &event) == -1) {
    switch (errno) {
    case EINVAL:
      return ERR(Void, Errors::invalid_operation);
    case ENOENT:
      return ERR(Void, Errors::fd_not_registered);
    case ENOMEM:
      return ERR(Void, Errors::out_of_mem);
    case EPERM:
      return ERR(Void, Errors::not_supported);
    default:
      return ERR(Void, "an unknown error occured during EPOLL_CTL_MOD");
    }
  }
  return OK(Void, new Void);
}

Result<Void> EPoll::del_fd(const FileDescriptor &fd) {
  epoll_event event = {};
  event.data.fd = fd._fd;
  if (epoll_ctl(_fd->_fd, EPOLL_CTL_DEL, fd._fd, &event) == -1) {
    switch (errno) {
    case EINVAL:
      return ERR(Void, Errors::invalid_operation);
    case ENOENT:
      return ERR(Void, Errors::fd_not_registered);
    case ENOMEM:
      return ERR(Void, Errors::out_of_mem);
    case EPERM:
      return ERR(Void, Errors::not_supported);
    default:
      return ERR(Void, "an unknown error occured during EPOLL_CTL_DEL");
    }
  }
  return OK(Void, new Void);
}

Result<Events> EPoll::wait(const int timeout_ms) {
  struct epoll_event *events = new struct epoll_event[_size];
  int n = epoll_wait(_fd->_fd, events, _size, timeout_ms);
  if (n == -1) {
    return ERR(Events, Errors::interrupted);
  }
  return Events::init(_events, n, events);
}
#endif
