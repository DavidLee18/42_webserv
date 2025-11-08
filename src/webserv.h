//
// Created by 이재현 on 2025-11-08.
//

#ifndef WEBSERV_H
#define WEBSERV_H
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <netdb.h>
#include <string>
#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#else
#include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t sig = 0;

void wrap_up(int);

class FileDescriptor {
    int _fd;
    public:
        explicit FileDescriptor(int);
        FileDescriptor move_from(FileDescriptor&);
        ~FileDescriptor();
        const int& operator*() const;
        void set_blocking(bool);
};

#ifdef __APPLE__
#else
/**
 * @class EPoll
 * @brief A simple epoll wrapper class.
 */
class EPoll {
    /**
     * @class Events
     * @brief A read-only iterator of epoll events.
     */
    class Events : public std::iterator<std::input_iterator_tag, struct epoll_event, long, const
                    epoll_event*, const epoll_event&> {
            size_t _curr;
            epoll_event *_Nonnull _events;
            size_t _len;
            public:
                explicit Events(size_t);
                ~Events();
                Events& operator++();
                Events operator++(int);
                bool operator==(const Events&) const;
                bool operator!=(const Events&) const;
                const epoll_event& operator*() const;
    };
    public:
        typedef Events Events;
        EPoll();
        ~EPoll();
        void add_fd(const FileDescriptor& fd, epoll_event *_Nonnull event);
        void modify_fd(const FileDescriptor& fd, epoll_event *_Nonnull event);
        void del_fd(const FileDescriptor& fd, epoll_event *_Nonnull event);
        Events& wait(uint16_t timeout_ms);
};
#endif
#endif