#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include "exceptions.h"
#include <stdexcept>
#include <sys/socket.h>

#ifdef __APPLE__
class KQueue;
#else
class EPoll;
class Option;
#endif

class Event;

/**
 * @class FileDescriptor
 * @brief Lightweight RAII wrapper around a POSIX file descriptor.
 *
 * This class centralizes ownership and lifetime management of an integer file
 * descriptor (`int fd`). It provides:
 * - Deterministic resource management: the owned fd is closed in the
 *   destructor (if valid), preventing leaks.
 * - A small, explicit API for common operations used by the web server
 *   (e.g., creating non-blocking sockets, binding, toggling blocking mode,
 *   and exposing the raw fd for low-level syscalls and I/O multiplexers).
 * - Clear error semantics: methods that can fail either return `bool` (no
 *   exceptions) or throw strongly-typed exceptions that map from `errno`
 *   values, as documented on each method.
 *
 * Ownership and transfer semantics:
 * - An instance uniquely owns its fd. Copying is not supported; instead, this
 *   class offers move-like transfer via a constructor taking a non-const
 *   reference and an assignment operator that takes its argument by value.
 *   These operations transfer ownership to the target and invalidate the
 *   source (set to `-1`).
 * - The destructor is no-throw and idempotent with respect to the resource.
 *
 * Thread-safety:
 * - Individual `FileDescriptor` objects are not thread-safe when accessed
 *   concurrently by multiple threads. External synchronization is required if
 *   the same instance is shared across threads.
 *
 * Exceptions:
 * - API methods that wrap syscalls throw project-specific exceptions declared
 *   in `exceptions.h`. Each method documents the mapping from `errno` to those
 *   exception types. Creation helpers (e.g., `socket_new()`) and operations
 *   like `socket_bind()` provide strong exception-safety guarantees.
 *
 * Interoperability:
 * - Use `operator*()` to obtain a const reference to the raw fd for
 *   integration with system interfaces such as `read(2)`, `write(2)`,
 *   `epoll_ctl(2)`, or `kevent(2)`. Do not manually `close(2)` the raw fd;
 *   lifetime is managed by this class.
 * - Platform integration helpers are declared as friends:
 *   - On Apple platforms, integration uses kqueue (`KQueue`, `Event`).
 *   - On Linux and others, integration uses epoll (`EPoll`, `Event`, `Option`).
 *
 * Typical usage:
 * ```cpp
 * // Create a non-blocking IPv4 TCP socket and bind it to INADDR_ANY:8080
 * FileDescriptor srv = FileDescriptor::socket_new();
 * in_addr any; any.s_addr = htonl(INADDR_ANY);
 * srv.socket_bind(any, 8080);
 * // Now ready for listen(2)/accept(2) via the contained fd (*srv).
 * ```
 */
class FileDescriptor {
  int _fd;

public:
  /**
   * @brief Constructs a FileDescriptor with the given file descriptor value.
   *
   * This constructor takes an integer representing a file descriptor and wraps
   * it in the FileDescriptor class, ensuring proper management of the file
   * descriptor. Throws an InvalidFileDescriptorException if the provided file
   * descriptor is invalid.
   *
   * @param raw_fd An integer representing the file descriptor to manage.
   * @throw InvalidFileDescriptorException if the provided file descriptor is
   * invalid.
   */
  explicit FileDescriptor(int raw_fd) throw(InvalidFileDescriptorException);

  /**
   * @brief Create a new non-blocking IPv4 TCP socket and wrap it in a
   * FileDescriptor.
   *
   * This factory function creates a socket using `socket(AF_INET,
   * SOCK_STREAM | SOCK_NONBLOCK, 0)` and returns a `FileDescriptor` that takes
   * ownership of the resulting file descriptor. The returned socket is
   * configured as non-blocking from creation time.
   *
   * Resource management: the returned `FileDescriptor` closes the socket in
   * its destructor. Do not call `close(2)` manually on the underlying fd.
   *
   * Thread-safety: not thread-safe if the same `FileDescriptor` instance is
   * shared and mutated concurrently.
   *
   * Exception safety: strong guarantee. On failure, no resources are leaked
   * and one of the exceptions listed below is thrown.
   *
   * @return A `FileDescriptor` managing the newly created socket.
   *
   * @throw AccessDeniedException
   *   If creating the socket is not permitted for the current process/user
   *   (maps from `EACCES`).
   * @throw NotSupportedOperationException
   *   If the address family or protocol is not supported on this system
   *   (maps from `EAFNOSUPPORT`, `EPROTONOSUPPORT`).
   * @throw InvalidOperationException
   *   If invalid flags or parameters are supplied to `socket(2)` (maps from
   *   `EINVAL`).
   * @throw FdTooManyException
   *   If the process or system has exhausted the limit of open file
   *   descriptors (maps from `EMFILE`, `ENFILE`).
   * @throw OutOfMemoryException
   *   If the system cannot allocate necessary resources (maps from
   *   `ENOBUFS`, `ENOMEM`).
   *
   * Example:
   * ```cpp
   * FileDescriptor srv = FileDescriptor::socket_new();
   * // Optionally set additional options, then bind/listen...
   * ```
   */
  static FileDescriptor socket_new() throw(AccessDeniedException,
                                           NotSupportedOperationException,
                                           InvalidOperationException,
                                           FdTooManyException,
                                           OutOfMemoryException);

  /**
   * @brief Transfers ownership of a FileDescriptor from one instance to
   * another.
   *
   * This method moves the file descriptor from the given FileDescriptor
   * instance into a new one, leaving the source FileDescriptor in a valid but
   * unspecified state. Use this function to safely transfer the responsibility
   * of managing a file descriptor.
   *
   * @param other The source FileDescriptor instance from which the file
   * descriptor will be moved.
   * @return A new FileDescriptor instance that takes ownership of the file
   * descriptor.
   * @throw InvalidFileDescriptorException if the provided FileDescriptor is
   * invalid or cannot be moved.
   */
  FileDescriptor(FileDescriptor &other) throw(InvalidFileDescriptorException);

  /**
   * @brief Transfers ownership of a FileDescriptor from one instance to
   * another.
   *
   * This method moves the file descriptor from the given FileDescriptor
   * instance into a new one, leaving the source FileDescriptor in a valid but
   * unspecified state. Use this function to safely transfer the responsibility
   * of managing a file descriptor.
   *
   * @param other The source FileDescriptor instance from which the file
   * descriptor will be moved.
   * @return A new FileDescriptor instance that takes ownership of the file
   * descriptor.
   * @throw InvalidFileDescriptorException if the provided FileDescriptor is
   * invalid or cannot be moved.
   */
  FileDescriptor &
  operator=(FileDescriptor other) throw(InvalidFileDescriptorException);

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
  const int &operator*() const;

  /**
   * @brief Sets blocking or non-blocking mode on the underlying file
   * descriptor.
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
   * @param blocking If true, set blocking mode; if false, set non-blocking
   * mode.
   * @return True if the operation was successful, false otherwise.
   */
  bool set_blocking(bool blocking);

  /**
   * @brief Bind the underlying IPv4 socket to a specific address and port.
   *
   * Associates this `FileDescriptor`'s socket with the given IPv4 address and
   * TCP port by calling `bind(2)`. The socket must have been created
   * beforehand (e.g., via `FileDescriptor::socket_new()`). On success, the
   * socket is ready to be put into a listening state with `listen(2)` for
   * server use, or used for outgoing connections that require an explicit
   * local address/port.
   *
   * Behavior:
   * - Binds using `sockaddr_in{AF_INET, addr, htons(port)}`.
   * - Does not modify socket options (e.g., `SO_REUSEADDR`); configure them
   *   separately before calling this method if needed.
   *
   * Resource management: this call does not take ownership of external
   * resources; the `FileDescriptor` continues to manage only its fd.
   *
   * Thread-safety: not thread-safe if the same `FileDescriptor` instance is
   * accessed concurrently.
   *
   * Exception safety: strong guarantee. On failure, the socket's state with
   * respect to binding remains unchanged and one of the exceptions below is
   * thrown.
   *
   * @param addr IPv4 address to bind to (e.g., `in_addr{INADDR_ANY}` for all
   *             interfaces).
   * @param port TCP port in host byte order.
   *
   * @throw AccessDeniedException
   *   Insufficient privileges to bind to the requested address/port
   *   (maps from `EACCES`).
   * @throw AddressNotAvailableException
   *   Address already in use or not available on this host (maps from
   *   `EADDRINUSE`, `EADDRNOTAVAIL`).
   * @throw InvalidFileDescriptorException
   *   The underlying fd is invalid or not a socket (maps from `EBADF`,
   *   `ENOTSOCK`).
   * @throw InvalidOperationException
   *   Invalid arguments or socket state for bind (maps from `EINVAL`).
   * @throw AddressFaultException
   *   The address pointer refers to invalid memory (maps from `EFAULT`).
   * @throw AddressLoopException
   *   Too many symbolic links were encountered resolving a path-like address
   *   (maps from `ELOOP`).
   * @throw NameTooLongException
   *   The provided address name is too long (maps from `ENAMETOOLONG`).
   * @throw NotFoundException
   *   A component of the address path does not exist or is not a directory
   *   (maps from `ENOENT`, `ENOTDIR`).
   * @throw OutOfMemoryException
   *   Insufficient kernel memory to complete the operation (maps from
   *   `ENOMEM`).
   * @throw ReadOnlyFileSystemException
   *   The operation is not permitted on a read-only filesystem (maps from
   *   `EROFS`).
   *
   * Example:
   * ```cpp
   * FileDescriptor srv = FileDescriptor::socket_new();
   * // Optional: set SO_REUSEADDR, etc.
   * in_addr any; any.s_addr = htonl(INADDR_ANY);
   * srv.socket_bind(any, 8080);
   * // Now: listen(...);
   * ```
   */
  void socket_bind(struct in_addr addr, unsigned short port) throw(
      AccessDeniedException, AddressNotAvailableException,
      InvalidFileDescriptorException, InvalidOperationException,
      AddressFaultException, AddressLoopException, NameTooLongException,
      NotFoundException, OutOfMemoryException, ReadOnlyFileSystemException);

  /**
   * @brief Put the bound socket into a passive listening state.
   *
   * Invokes `listen(2)` on the underlying socket so it can accept incoming
   * connection requests. This should typically be called after a successful
   * `socket_bind()` for server sockets. On success, the file descriptor moves
   * into a passive (listening) state and may be used with `accept(2)`.
   *
   * Behavior:
   * - Calls `listen(fd, backlog)` where `backlog` specifies the maximum length
   *   of the pending connection queue. The effective queue length may be
   *   capped by the OS (e.g., `SOMAXCONN`).
   * - Does not modify any socket options; configure them prior to calling this
   *   method if needed (e.g., `SO_REUSEADDR`).
   *
   * Preconditions:
   * - The underlying fd must be a valid stream socket (e.g., IPv4 TCP).
   * - For server sockets, `socket_bind()` should have been called first to
   *   select a local address/port.
   *
   * Resource management: no ownership changes occur; this object continues to
   * manage the fd lifetime.
   *
   * Thread-safety: not thread-safe when the same instance is accessed
   * concurrently.
   *
   * Exception safety: strong guarantee. On failure, the socket remains in its
   * previous state and one of the exceptions below is thrown.
   *
   * @param backlog Maximum number of pending connections in the accept queue
   *                (subject to OS-imposed limits such as `SOMAXCONN`).
   *
   * @throw AddressNotAvailableException
   *   The address is already in use or otherwise unavailable for listening
   *   (maps from `EADDRINUSE`).
   * @throw InvalidFileDescriptorException
   *   The fd is invalid or not a socket (maps from `EBADF`, `ENOTSOCK`).
   * @throw NotSupportedOperationException
   *   The socket type does not support listening (maps from `EOPNOTSUPP`).
   *
   * Example:
   * ```cpp
   * FileDescriptor srv = FileDescriptor::socket_new();
   * // Optional: set SO_REUSEADDR, etc.
   * in_addr any; any.s_addr = htonl(INADDR_ANY);
   * srv.socket_bind(any, 8080);
   * srv.socket_listen(128);
   * // Now ready to accept connections on *srv
   * ```
   */
  void
  socket_listen(unsigned short backlog) throw(AddressNotAvailableException,
                                              InvalidFileDescriptorException,
                                              NotSupportedOperationException);

  FileDescriptor socket_accept(struct sockaddr *addr, socklen_t *len) throw(
      TryAgainException, ConnectionAbortedException,
      InvalidFileDescriptorException, std::invalid_argument,
      AddressFaultException, InterruptedException, FdTooManyException,
      OutOfMemoryException, NotSupportedOperationException,
      AccessDeniedException);

  bool operator==(const int &other) const { return _fd == other; }
  bool operator==(const FileDescriptor &other) const {
    return _fd == other._fd;
  }
  bool operator!=(const int &other) const { return !(*this == other); }
  bool operator!=(const FileDescriptor &other) const {
    return !(*this == other);
  }
  friend bool operator==(const int &lhs, const FileDescriptor &rhs);
  friend bool operator!=(const int &lhs, const FileDescriptor &rhs);
};

#endif // FILE_DESCRIPTOR_H
