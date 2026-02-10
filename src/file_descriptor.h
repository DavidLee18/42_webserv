#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

// #include "http_1_1.h"
#include "http_1_1.h"
#include "result.h"
#include <sys/socket.h>

class Event;

class FileDescriptor {
  int _fd;
  FILE *fp;
  
  FileDescriptor() : _fd(-1), fp(NULL) {}

public:
  static Result<FileDescriptor> from_raw(int);

  static Result<FileDescriptor> socket_new();
  
  static Result<FileDescriptor> open_file(std::string const &);
  
  // Move-like copy constructor: transfers ownership from other
  FileDescriptor(const FileDescriptor &other);

  // Move-like assignment operator: transfers ownership from other
  FileDescriptor &operator=(const FileDescriptor &other);

  ~FileDescriptor();

  Result<Void> socket_bind(struct in_addr addr, unsigned short port);

  Result<Void> socket_listen(unsigned short backlog);

  Result<FileDescriptor> socket_accept(struct sockaddr *addr, socklen_t *len);

  Result<ssize_t> sock_recv(void *buf, size_t size);

  Result<Http::PartialString> try_read_to_end();

  /**
   * @brief Sets the file descriptor to non-blocking mode.
   *
   * This method is REQUIRED when using sockets with EPoll in edge-triggered
   * (ET) mode.
   *
   * Edge-triggered mode only notifies once when a state change occurs. To
   * properly handle all available data, the application must read/accept in a
   * loop until EAGAIN/EWOULDBLOCK is returned. Without non-blocking mode, the
   * socket operations would block indefinitely, freezing the event loop and
   * preventing other clients from being served.
   *
   * Without non-blocking mode:
   * - accept() could block when no connections are pending (ET doesn't
   * retrigger)
   * - recv() could block when no data is available (ET doesn't retrigger)
   * - The entire event loop would hang, making the server unresponsive
   *
   * With non-blocking mode:
   * - Operations return immediately with EAGAIN/EWOULDBLOCK when no data is
   * ready
   * - The event loop can process other file descriptors
   * - The server remains responsive to all clients
   *
   * This is a fundamental requirement of the edge-triggered + non-blocking I/O
   * pattern, which is the standard approach for scalable event-driven servers.
   *
   * @return Result<Void> Success or error message
   */
  Result<Void> set_nonblocking();

  Result<Void> set_socket_option(int level, int optname, const void *optval,
                                 socklen_t optlen);

  Result<ssize_t> sock_send(const void *buf, size_t size);

  Result<std::string> read_file_line();

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

  friend class EPoll;
};

#endif // FILE_DESCRIPTOR_H
