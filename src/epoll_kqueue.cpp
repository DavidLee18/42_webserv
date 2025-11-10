//
// Created by 이재현 on 2025-11-10.
//
#include "webserv.h"

#ifdef __APPLE__
#else
EPoll::Events::Events(const std::vector<FileDescriptor> &all_events, const size_t size,
    const epoll_event *events) throw(FdNotRegisteredException) : _all_events(all_events), _len(size), _curr(0) {
    _events = (Event*)operator new(sizeof(Event) * size);
    bool found = false;
    for (int i = 0; i < size; i++) {
        for (std::vector<FileDescriptor>::const_iterator it = all_events.begin(); it != all_events.end(); ++it) {
            if ((*it) == events[i].data.fd) {
                _events[i] = new(_events + i) Event(*it, events[i].events & EPOLLIN == 1,
                                                    events[i].events & EPOLLOUT == 1,
                                                    events[i].events & EPOLLRDHUP == 1,
                                                    events[i].events & EPOLLPRI == 1,
                                                    events[i].events & EPOLLERR == 1,
                                                    events[i].events & EPOLLHUP == 1);
                found = true;
                break;
            }
        }
        if (!found)
            throw FdNotRegisteredException();
    }
}

EPoll::Events::~Events() {
    for (int i = 0; i < _len; i++) {
        _events[i].~Event();
    }
    operator delete(_events);
}

bool EPoll::Events::is_end() const {
    return _curr >= _len;
}

EPoll::Events& EPoll::Events::operator++() throw(IteratorEndedException) {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    ++_curr;
    return *this;
}

EPoll::Events EPoll::Events::operator++(int) throw(IteratorEndedException) {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    EPoll::Events temp(_all_events, _len, _events);
    temp._curr = _curr++;
    return temp;
}

const Event& EPoll::Events::operator*() throw(IteratorEndedException) const {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    return _events[_curr];
}

#endif