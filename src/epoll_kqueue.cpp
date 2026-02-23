#include "webserv.h"

Result<Events> Events::init(const std::map<int, FileDescriptor> &all_events,
                            size_t size, const epoll_event *events) {
  Events es;
  es._len = size;
  es._curr = 0;
  es._events = static_cast<Event *>(operator new(sizeof(Event) * size));
  for (size_t i = 0; i < size; i++) {
    const FileDescriptor *fd = NULL;
    std::map<int, FileDescriptor>::const_iterator it =
        all_events.find(events[i].data.fd);
    if (it != all_events.end())
      fd = &it->second;
    if (fd == NULL) {
      operator delete((void *)es._events);
      return ERR(Events, Errors::not_found);
    }
    new ((void *)(es._events + i)) Event(fd, (events[i].events & EPOLLIN) != 0,
                                         (events[i].events & EPOLLOUT) != 0,
                                         (events[i].events & EPOLLRDHUP) != 0,
                                         (events[i].events & EPOLLPRI) != 0,
                                         (events[i].events & EPOLLERR) != 0,
                                         (events[i].events & EPOLLHUP) != 0);
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
  return OKV;
}

Result<const Event *> Events::operator*() const {
  if (_curr >= _len) {
    return ERR(const Event *, Errors::iter_ended);
  }
  const Event *evp = _events + _curr;
  return OK(const Event *, evp);
}

Result<EPoll> EPoll::create(unsigned short sz) {
  int fd = epoll_create(static_cast<int>(sz));
  if (fd < 0) {
    switch (errno) {
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
  EPoll ep;
  ep._size = sz;
  Result<FileDescriptor> rfdesc = FileDescriptor::from_raw(fd);
  if (!rfdesc.error().empty()) {
    return ERR(EPoll, rfdesc.error());
  }
  FileDescriptor fdesc = rfdesc.value();
  ep._fd = fdesc;
  return OK(EPoll, ep);
}

Result<int> EPoll::add_fd(FileDescriptor fd, const Event &ev,
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
  if (epoll_ctl(_fd._fd, EPOLL_CTL_ADD, fd._fd, &event) == -1) {
    switch (errno) {
    case EEXIST:
      return ERR(int, "this fd is already registered to this epoll");
    case EINVAL:
      return ERR(int, Errors::invalid_fd);
    case ELOOP:
      return ERR(int, Errors::epoll_loop);
    case ENOMEM:
      return ERR(int, Errors::out_of_mem);
    case ENOSPC:
      return ERR(int, Errors::epoll_full);
    case EPERM:
      return ERR(int, Errors::not_supported);
    default:
      return ERR(int, "an unknown error occured during EPOLL_CTL_ADD");
    }
  }
  int raw = fd._fd;
  _events.push_back(fd);

  // Return the raw file descriptor integer (avoids storing pointers into the
  // vector, which could be invalidated on reallocation)
  return OK(int, raw);
}

Result<Void> EPoll::modify_fd(FileDescriptor &fd, const Event &ev,
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
  if (epoll_ctl(_fd._fd, EPOLL_CTL_MOD, fd._fd, &event) == -1) {
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
  return OKV;
}

Result<Void> EPoll::del_fd(const FileDescriptor &fd) {
  epoll_event event = {};
  event.data.fd = fd._fd;
  if (epoll_ctl(_fd._fd, EPOLL_CTL_DEL, fd._fd, &event) == -1) {
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
  return OKV;
}

Result<Events> EPoll::wait(const int timeout_ms) {
  struct epoll_event *events = new struct epoll_event[_size];
  int n = epoll_wait(_fd._fd, events, _size, timeout_ms);
  if (n == -1) {
    delete[] events;
    if (errno == EINTR)
      return ERR(Events, Errors::interrupted);
    return ERR(Events, std::string("epoll_wait failed: ") + strerror(errno));
  }
  return Events::init(_events, static_cast<size_t>(n), events);
}
