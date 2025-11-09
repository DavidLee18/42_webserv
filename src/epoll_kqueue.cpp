//
// Created by 이재현 on 2025-11-10.
//
#include "webserv.h"

#ifdef __APPLE__
#else
EPoll::Events::Events(const size_t size, const epoll_event* events) : _events(events), _len(size), _curr(0) {}

EPoll::Events::~Events() { delete[] _events; }

EPoll::Events EPoll::Events::end() const {
    EPoll::Events end(_len, _events);
    end._curr = _len;
    return end;
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
    EPoll::Events temp(_len, _events);
    temp._curr = _curr++;
    return temp;
}

bool EPoll::Events::operator==(const EPoll::Events& other) const {
    return _curr == other._curr && _events == other._events && _len == other._len;
}

bool EPoll::Events::operator!=(const EPoll::Events& other) const {
    return !(*this == other);
}

const Event& EPoll::Events::operator*() throw(IteratorEndedException) const {
    if (_curr >= _len) {
        throw IteratorEndedException();
    }
    return event_from_raw(_events[_curr]);
}

EPoll::Event EPoll::event_from_raw(const epoll_event& event) {
    // TODO
}

#endif