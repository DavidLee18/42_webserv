#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

// #include "http_1_1.h"
#include "http_1_1.h"
#include "result.h"
#include <sys/socket.h>

#ifdef __APPLE__
class KQueue;
#else
class EPoll;
class Option;
#endif

class Event;

class FileDescriptor {
  int _fd;

  void set_fd(int fd) { _fd = fd; }
  FileDescriptor() { _fd = -1; }

public:
  static Result<FileDescriptor> from_raw(int);

  static Result<FileDescriptor> socket_new();

  // Move-like copy constructor: transfers ownership from other
  FileDescriptor(const FileDescriptor &other);

  // Move-like assignment operator: transfers ownership from other
  FileDescriptor& operator=(const FileDescriptor &other);

  ~FileDescriptor();

  Result<Void> socket_bind(struct in_addr addr, unsigned short port);

  Result<Void> socket_listen(unsigned short backlog);

  Result<FileDescriptor> socket_accept(struct sockaddr *addr, socklen_t *len);

  Result<ssize_t> sock_recv(void *buf, size_t size);

  Result<Http::PartialString> try_read_to_end();

  int get_fd() const { return _fd; }

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
