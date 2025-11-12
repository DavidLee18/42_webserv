//
// Created by 이재현 on 2025-11-10.
//
#include "webserv.h"

#ifdef __APPLE__
const FileDescriptor& add_event(FileDescriptor fd, KQueue& kq, const Event& ev) throw(InvalidFileDescriptorException) {
    if (fd != ev.fd)
        throw InvalidFileDescriptorException();
    struct kevent event = {};
    if (ev.read)
    if (kevent(*kq._fd, &event, 1, NULL, 0, NULL) == -1)
        throw std::runtime_error("kevent failed");
    kq._events.push(fd);
    return kq._events[kq._events.size() - 1];
}

void del_event(const FileDescriptor& fd, KQueue& kq, const Event& event) throw(InvalidFileDescriptorException) {
    /* TODO */
    (void)fd;
    (void)kq;
    (void)event;
}
#else
Events::Events(const Vec<FileDescriptor>& all_events, const size_t size,
    const epoll_event *events) throw(FdNotRegisteredException) : _all_events(all_events), _len(size), _curr(0) {
    _events = static_cast<Event *>(operator new(sizeof(Event) * size));
    for (int i = 0; i < size; i++) {
        if (all_events.elem(events[i].data.fd)) {
            _events[i] = new(_events + i) Event(*it, events[i].events & EPOLLIN == 1,
                                                events[i].events & EPOLLOUT == 1,
                                                events[i].events & EPOLLRDHUP == 1,
                                                events[i].events & EPOLLPRI == 1,
                                                events[i].events & EPOLLERR == 1,
                                                events[i].events & EPOLLHUP == 1);
            } else {
                throw FdNotRegisteredException();
            }
        }
    }
}

Events::~Events() {
    for (int i = 0; i < _len; i++) {
        _events[i].~Event();
    }
    operator delete(_events);
}

bool Events::is_end() const {
    return _curr >= _len;
}

Events& Events::operator++() throw(IteratorEndedException) {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    ++_curr;
    return *this;
}

Events EPoll::Events::operator++(int) throw(IteratorEndedException) {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    Events temp(_all_events, _len, _events);
    temp._curr = _curr++;
    return temp;
}

const Event& Events::operator*() throw(IteratorEndedException) const {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    return _events[_curr];
}

EPoll::EPoll(size_t size) throw(InvalidFileDescriptorException): _size(size), _events(), _fd(-1) {
    int fd = epoll_create(size);
    if (fd == -1) {
        throw InvalidFileDescriptorException();
    }
    _fd = FileDescriptor(fd);
}

const FileDescriptor& add_fd(FileDescriptor fd, EPoll& ep, const Event& ev,
    const Option& op) throw(InvalidOperationException, EPollLoopException, FdNotRegisteredException,
        OutOfMemoryException, EPollFullException, NotSupportedOperationException) {
    epoll_event event = {};
    if (ev.read)
        event.events |= EPOLLIN;
    if (ev.write)
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
    if (epoll_ctl(*(ep._fd), EPOLL_CTL_ADD, *fd, &event) == -1) {
        switch (errno) {
            case EEXIST: throw InvalidOperationException();
            case EINVAL: throw InvalidOperationException();
            case ELOOP: throw EPollLoopException();
            case ENOENT: throw FdNotRegisteredException();
            case ENOMEM: throw OutOfMemoryException();
            case ENOSPC: throw EPollFullException();
            case EPERM: throw NotSupportedOperationException();
        }
    } else {
        ep._events.push(fd);
        return ep._events[ep._events.size() - 1];
    }
}

void modify_fd(const FileDescriptor& fd, EPoll& ep, const Event& ev, const Option& op) throw(InvalidOperationException,
    FdNotRegisteredException, OutOfMemoryException, NotSupportedOperationException) {
    epoll_event event = {};
    if (ev.read)
        event.events |= EPOLLIN;
    if (ev.write)
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
    if (epoll_ctl(*(ep._fd), EPOLL_CTL_MOD, *fd, &event) == -1) {
        switch (errno) {
            case EINVAL: throw InvalidOperationException();
            case ENOENT: throw FdNotRegisteredException();
            case ENOMEM: throw OutOfMemoryException();
            case EPERM: throw NotSupportedOperationException();
        }
    }
}

void del_fd(const FileDescriptor& fd, EPoll& ep, const Event& ev, const Option& op) throw(InvalidOperationException,
    FdNotRegisteredException, OutOfMemoryException, NotSupportedOperationException) {
    epoll_event event = {};
    event.data.fd = *fd;
    if (epoll_ctl(*(ep._fd), EPOLL_CTL_DEL, *fd, &event) == -1) {
        switch (errno) {
            case EINVAL: throw InvalidOperationException();
            case ENOENT: throw FdNotRegisteredException();
            case ENOMEM: throw OutOfMemoryException();
            case EPERM: throw NotSupportedOperationException();
        }
    }
}

Events EPoll::wait(const int timeout_ms) throw(InterruptedException) {
    struct epoll_event events[_size];
    int n = epoll_wait(_fd, events, _size, timeout_ms);
    if (n == -1) {
        throw InterruptedException();
    }
    return Events(_events, n, events);
}
#endif