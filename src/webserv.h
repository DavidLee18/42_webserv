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

void wrap_up(int) throw();

class InvalidFileDescriptorException : public std::exception {
    public:
        const char *_Nonnull what() const throw() { return "Invalid file descriptor"; }
};

/**
 * @class FileDescriptor
 * @brief A class to manage and encapsulate file descriptors.
 *
 * FileDescriptor provides safe ownership and management of file descriptors
 * used for system-level I/O operations. It ensures proper handling of resources
 * and prevents common errors such as resource leaks.
 */
class FileDescriptor {
    int _fd;
    public:
        /**
         * @brief Constructs a FileDescriptor with the given file descriptor value.
         *
         * This constructor takes an integer representing a file descriptor and wraps it
         * in the FileDescriptor class, ensuring proper management of the file descriptor.
         * Throws an InvalidFileDescriptorException if the provided file descriptor is invalid.
         *
         * @param raw_fd An integer representing the file descriptor to manage.
         * @throw InvalidFileDescriptorException if the provided file descriptor is invalid.
         */
        explicit FileDescriptor(int raw_fd) throw(InvalidFileDescriptorException);

        /**
         * @brief Transfers ownership of a FileDescriptor from one instance to another.
         *
         * This method moves the file descriptor from the given FileDescriptor instance
         * into a new one, leaving the source FileDescriptor in a valid but unspecified state.
         * Use this function to safely transfer the responsibility of managing a file descriptor.
         *
         * @param other The source FileDescriptor instance from which the file descriptor will be moved.
         * @return A new FileDescriptor instance that takes ownership of the file descriptor.
         * @throw InvalidFileDescriptorException if the provided FileDescriptor is invalid or cannot be moved.
         */
        static FileDescriptor move_from(FileDescriptor& other) throw(InvalidFileDescriptorException);

        /**
         * @brief Destructor that releases the owned file descriptor.
         *
         * Closes the underlying file descriptor if it is valid (>= 0). Any error
         * returned by close(2) is intentionally ignored, as destructors must not
         * throw. The destructor is idempotent with respect to the managed resource:
         * it will only attempt to close once per instance.
         *
         * Thread-safety: not thread-safe if multiple threads manipulate the same
         * FileDescriptor instance concurrently.
         *
         * Exception safety: no-throw.
         */
        ~FileDescriptor();
        
        /**
         * @brief Returns a const reference to the underlying file descriptor.
         *
         * Use this to get the raw integer file descriptor for interoperability
         * with low-level system calls (e.g., read(2), write(2), epoll_ctl(2),
         * kevent(2)). The returned reference remains valid for the lifetime of
         * this FileDescriptor instance. Do not call close(2) on the value
         * obtained via this operator; ownership and lifetime are managed by
         * FileDescriptor.
         *
         * Thread-safety: not thread-safe if the same FileDescriptor instance is
         * accessed concurrently.
         *
         * Exception safety: no-throw.
         *
         * @return Const reference to the managed file descriptor integer.
         */
        const int& operator*() const;

        /**
         * @brief Sets blocking or non-blocking mode on the underlying file descriptor.
         *
         * When @p blocking is false, the function enables O_NONBLOCK; when true,
         * it clears O_NONBLOCK to restore blocking behavior. On failure, the
         * descriptor's previous mode is left unchanged and errno is set by the
         * underlying system calls (e.g., fcntl(2)).
         *
         * Thread-safety: not thread-safe if multiple threads manipulate the same
         * FileDescriptor instance concurrently.
         *
         * Exception safety: no-throw.
         *
         * @param blocking If true, set blocking mode; if false, set non-blocking mode.
         * @return True if the operation was successful, false otherwise.
         */
        bool set_blocking(bool blocking) const;
};

class IteratorEndedException : public std::exception {
    public:
        const char *_Nonnull what() const throw() { return "Iterator reached end"; }
};

class InvariantViolationException : public std::exception {
    public:
        const char *_Nonnull what() const throw() { return "Invariant violation"; }
};

class InterruptedException : public std::exception {
    public:
        const char *_Nonnull what() const throw() { return "Operation interrupted"; }
};

#ifdef __APPLE__
/**
 * @class KQueue
 * @brief A class for managing and interacting with kernel event queues.
 *
 * The KQueue class encapsulates functionalities for creating and managing
 * kernel queues, primarily used for monitoring and handling I/O events
 * and other event-driven scenarios. It facilitates efficient event
 * notification mechanisms within applications.
 */
class KQueue {
    /**
     * @class Event
     * @brief Represents an event with associated file descriptor and operation flags.
     *
     * Event is used to describe the state and operations associated with a file descriptor
     * in an I/O event-driven context. It includes flags indicating whether the event
     * is for reading, writing, or deletion.
     */
    class Event {
        public:
            const FileDescriptor& _fd;
            const bool _read;
            const bool _write;
            const bool _delete;
            Event(const FileDescriptor &fd, const bool read, const bool write, const bool is_delete) : _fd(fd),
                _read(read), _write(write),
                _delete(is_delete) {}
    };

    /**
     * @class Events
     * @brief An iterator class to handle and represent a collection of events.
     *
     * The Events class provides functionality to store, manage, and manipulate
     * a set of events. It offers operation to query events, ensuring
     * efficient and structured event management within an application.
     */
    class Events : public std::iterator<std::input_iterator_tag, Event, long, const Event*, const Event&> {
        size_t _curr;
        Event *_Nonnull _events;
        size_t _len;
        public:
            explicit Events(size_t);
            ~Events();
            Events end() const;
            Events& operator++() throw(IteratorEndedException);
            Events operator++(int) throw(IteratorEndedException);
            bool operator==(const Events&) const;
            bool operator!=(const Events&) const;
            const Event& operator*() const;
    };
    size_t _cap;
    FileDescriptor _fd;
    public:
        typedef Event Event;
        explicit KQueue(size_t);
        ~KQueue();
        void add_event(const Event&);
        void del_event(const Event&);
        Events& wait(int timeout_ms);
};
#else
/**
 * @class EPoll
 * @brief A simple epoll wrapper class.
 */
class EPoll {
    /**
     * @class Event
     * @brief A class to encapsulate event data for epoll-based operations.
     *
     * The Event class represents an individual event associated with a file descriptor,
     * capturing various event flags such as readability, writability, and error states.
     * It is used for monitoring the state and activity of file descriptors in the
     * context of an epoll event loop.
     */
    class Event {
        public:
            Event(const FileDescriptor &fd, const bool in, const bool out, const bool rdhup, const bool pri,
                  const bool err, const bool hup) : _fd(fd), _in(in), _out(out), _rdhup(rdhup), _pri(pri), _err(err),
                                                    _hup(hup) {}
            const FileDescriptor& _fd;
            const bool _in;
            const bool _out;
            const bool _rdhup;
            const bool _pri;
            const bool _err;
            const bool _hup;
    };

    /**
     * @class Events
     * @brief An input iterator class for traversing epoll events.
     *
     * The Events class provides an interface to iterate over a collection of
     * epoll events. It encapsulates event data and supports standard input iterator
     * operations such as increment, equality comparison, and dereferencing.
     *
     * This class is specifically designed to work with epoll-based event collections,
     * facilitating efficient iteration over the events with proper handling of
     * underlying resources.
     */
    class Events : public std::iterator<std::input_iterator_tag, Event, long, const Event*, const Event&> {
            size_t _curr;
            epoll_event *_Nonnull _events;
            size_t _len;
            Event event_from_raw(const epoll_event&);
            public:
                Events(size_t, const epoll_event*);
                ~Events();
                Events end() const;
                Events& operator++() throw(IteratorEndedException);
                Events operator++(int) throw(IteratorEndedException);
                bool operator==(const Events&) const;
                bool operator!=(const Events&) const;
                const Event& operator*() throw(IteratorEndedException) const;
    };
    FileDescriptor _fd;
    size_t _size;
    public:
        /**
         * @class Option
         * @brief A class representing configuration options for epoll events.
         *
         * Option encapsulates a set of flags that define the behavior of epoll
         * operations, such as edge-triggered mode, one-shot behavior, wake-up events,
         * and exclusive event handling.
         */
        class Option {
            public:
                Option(const bool et, const bool oneshot, const bool wakeup, const bool exclusive);
                const bool _et;
                const bool _oneshot;
                const bool _wakeup;
                const bool _exclusive;
        };
        EPoll(size_t size);
        ~EPoll();
        void add_fd(const FileDescriptor& fd, const Option& option);
        void modify_fd(const FileDescriptor& fd, const Option& option);
        void del_fd(const FileDescriptor& fd, const Option& option);
        Events& wait(const int timeout_ms) throw(InterruptedException, InvariantViolationException);
};
#endif
#endif