//
// Created by 이재현 on 2025-11-12.
//

#ifndef INC_42_WEBSERV_FILE_DESCRIPTOR_H
#define INC_42_WEBSERV_FILE_DESCRIPTOR_H

#include "exceptions.h"

#ifdef __APPLE__
class KQueue;
#else
class EPoll;
class Option;
#endif

class Event;

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
        FileDescriptor(FileDescriptor& other) throw(InvalidFileDescriptorException);

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
        FileDescriptor& operator=(FileDescriptor other) throw(InvalidFileDescriptorException);

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
        bool set_blocking(bool blocking);

        bool operator==(const int& other) const { return _fd == other; }
        bool operator==(const FileDescriptor& other) const { return _fd == other._fd; }
        bool operator!=(const int& other) const { return !(*this == other); }
        bool operator!=(const FileDescriptor& other) const { return !(*this == other); }
        friend bool operator==(const int& lhs, const FileDescriptor& rhs);
        friend bool operator!=(const int& lhs, const FileDescriptor& rhs);

    #ifdef __APPLE__
        friend const FileDescriptor& add_event(FileDescriptor, const KQueue&,
            const Event&) throw(InvalidFileDescriptorException);
        friend void del_event(const FileDescriptor&, KQueue&, const Event&) throw(InvalidFileDescriptorException);
    #else
        friend const FileDescriptor& add_fd(FileDescriptor, EPoll&, const Event&,
            const Option&) throw(InvalidFileDescriptorException, EPollLoopException, FdNotRegisteredException,
                OutOfMemoryException, EPollFullException, NotSupportedOperationException);
        friend void modify_fd(const FileDescriptor&, EPoll&, const Event&, const Option&);
        friend void del_fd(const FileDescriptor&, EPoll&, const Event&, const Option&);
    #endif
};

#endif //INC_42_WEBSERV_FILE_DESCRIPTOR_H