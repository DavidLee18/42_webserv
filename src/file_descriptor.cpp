#include "webserv.h"

Result<FileDescriptor> FileDescriptor::socket_new() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    switch (errno) {
    case EACCES:
      return ERR(FileDescriptor, Errors::access_denied);
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
      return ERR(FileDescriptor, Errors::not_supported);
    case EINVAL:
      return ERR(FileDescriptor, Errors::invalid_operation);
    case EMFILE:
    case ENFILE:
      return ERR(FileDescriptor, Errors::fd_too_many);
    case ENOBUFS:
    case ENOMEM:
      return ERR(FileDescriptor, Errors::out_of_mem);
    }
  }
  FileDescriptor fd;
  fd._fd = sock;
  return OK(FileDescriptor, fd);
}

Result<FileDescriptor> FileDescriptor::from_raw(int raw_fd) {
  if (raw_fd < 0)
    return ERR(FileDescriptor, Errors::invalid_fd);
  FileDescriptor fd;
  fd._fd = raw_fd;
  return OK(FileDescriptor, fd);
}

Result<FileDescriptor> FileDescriptor::open_file(std::string const &path) {
  int _fd = open(path.c_str(), O_RDONLY);
  if (_fd < 0)
    return ERR(FileDescriptor,
               Errors::invalid_fd); // TODO: specify error kind and message.
  FileDescriptor fd;
  fd._fd = _fd;
  FILE *fp = fdopen(_fd, "r");
  if (fp == NULL) {
    switch (errno) {
    case EMFILE:
      return ERR(FileDescriptor, Errors::stream_too_many);
    case EBADF:
      return ERR(FileDescriptor, Errors::invalid_fd);
    case ENOMEM:
      return ERR(FileDescriptor, Errors::out_of_mem);
    default:
      std::stringstream ss("an unknown error occured; errno: ");
      ss << errno;
      return ERR(FileDescriptor, ss.str());
    }
  }
  fd.fp = fp;
  return OK(FileDescriptor, fd);
}

// Move-like copy constructor: transfers ownership from other
FileDescriptor::FileDescriptor(const FileDescriptor &other)
    : _fd(other._fd), fp(other.fp) {
  // Invalidate source to transfer ownership (cast away const for move
  // semantics)
  FileDescriptor other_ = const_cast<FileDescriptor &>(other);
  other_._fd = -1;
  other_.fp = NULL;
}

// Move-like assignment operator: transfers ownership from other
FileDescriptor &FileDescriptor::operator=(const FileDescriptor &other) {
  if (this != &other) {
    // Close current fd if valid
    if (fp != NULL)
      std::fclose(fp);
    else if (_fd >= 0)
      close(_fd);

    // Transfer ownership
    _fd = other._fd;
    fp = other.fp;
    // Invalidate source (cast away const for move semantics)
    FileDescriptor &other_ = const_cast<FileDescriptor &>(other);
    other_._fd = -1;
    other_.fp = NULL;
  }
  return *this;
}

FileDescriptor::~FileDescriptor() {
  if (fp != NULL)
    std::fclose(fp);
  else if (_fd >= 0)
    close(_fd);
}

Result<Void> FileDescriptor::socket_bind(struct in_addr addr,
                                         unsigned short port) {
  sockaddr_in _addr;
  std::memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  _addr.sin_addr = addr;
  _addr.sin_port = htons(port);
  if (bind(_fd, reinterpret_cast<const sockaddr *>(&_addr), sizeof(_addr)) <
      0) {
    switch (errno) {
    case EACCES:
      return ERR(Void, Errors::access_denied);
    case EADDRINUSE:
    case EADDRNOTAVAIL:
      return ERR(Void, Errors::addr_not_available);
    case EBADF:
    case ENOTSOCK:
      return ERR(Void, Errors::invalid_fd);
    case EINVAL:
      return ERR(Void, Errors::invalid_operation);
    case EFAULT:
      return ERR(Void, Errors::address_fault);
    case ELOOP:
      return ERR(Void, Errors::addr_loop);
    case ENAMETOOLONG:
      return ERR(Void, Errors::name_too_long);
    case ENOENT:
    case ENOTDIR:
      return ERR(Void, Errors::not_found);
    case ENOMEM:
      return ERR(Void, Errors::out_of_mem);
    case EROFS:
      return ERR(Void, Errors::readonly_filesys);
    }
  }
  return OKV;
}

Result<Void> FileDescriptor::socket_listen(unsigned short backlog) {
  if (listen(_fd, backlog) < 0) {
    switch (errno) {
    case EADDRINUSE:
      return ERR(Void, Errors::addr_not_available);
    case EBADF:
    case ENOTSOCK:
      return ERR(Void, Errors::invalid_fd);
    case EOPNOTSUPP:
      return ERR(Void, Errors::not_supported);
    }
  }
  return OKV;
}

Result<FileDescriptor> FileDescriptor::socket_accept(struct sockaddr *addr,
                                                     socklen_t *len) {
  int fd = accept(_fd, addr, len);
  if (fd >= 0) {
    FileDescriptor fd_;
    fd_._fd = fd;
    return OK(FileDescriptor, fd_);
  }
  switch (errno) {
  case EWOULDBLOCK:
    return ERR(FileDescriptor, Errors::try_again);
  case EBADF:
  case ENOTSOCK:
    return ERR(FileDescriptor, Errors::invalid_fd);
  case ECONNABORTED:
    return ERR(FileDescriptor, Errors::conn_aborted);
  case EFAULT:
    return ERR(FileDescriptor, Errors::address_fault);
  case EINTR:
    return ERR(FileDescriptor, Errors::interrupted);
  case EINVAL:
    return ERR(
        FileDescriptor,
        "Socket is not listening for connections, or addrlen is invalid.");
  case EMFILE:
  case ENFILE:
    return ERR(FileDescriptor, Errors::fd_too_many);
  case ENOBUFS:
  case ENOMEM:
    return ERR(FileDescriptor, Errors::out_of_mem);
  case EOPNOTSUPP:
    return ERR(FileDescriptor,
               "The referenced socket is not of type SOCK_STREAM.");
  case EPERM:
    return ERR(FileDescriptor, Errors::access_denied);
  default:
    return ERR(FileDescriptor, "an unknown error occured during accept().");
  }
}

Result<ssize_t> FileDescriptor::sock_recv(void *buf, size_t size) {
  ssize_t res = recv(_fd, buf, size, 0);
  if (res < 0)
    return ERR(ssize_t, "`recv` failed");
  return OK(ssize_t, res);
}

Result<Http::PartialString> FileDescriptor::try_read_to_end() {
  std::stringstream ss;
  char buf[BUFFER_SIZE];

  Result<ssize_t> bytes = this->sock_recv(buf, BUFFER_SIZE);
  while (bytes.error().empty() && bytes.value() > 0) {
    ssize_t bs;
    TRY(Http::PartialString, ssize_t, bs, bytes)
    char *s = new char[static_cast<size_t>(bs + 1)];
    s = std::strncpy(s, buf, static_cast<size_t>(bs + 1));
    if (!(ss << s))
      return ERR(Http::PartialString, "string concat failed");
    delete[] s;
    bytes = this->sock_recv(buf, BUFFER_SIZE);
  }
  if (!bytes.error().empty())
    return ERR(Http::PartialString, bytes.error());
  char *s = new char[ss.str().length()];
  s = std::strcpy(s, ss.str().c_str());
  if (bytes.value() == 0) {
    return OK(Http::PartialString, Http::PartialString::full(s));
  }
  return OK(Http::PartialString, Http::PartialString::partial(s));
}

Result<Void> FileDescriptor::set_nonblocking() {
  int flags = fcntl(_fd, F_GETFL, 0);
  if (flags < 0) {
    return ERR(Void, "Failed to get file descriptor flags");
  }
  if (fcntl(_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return ERR(Void, "Failed to set non-blocking mode");
  }
  return OKV;
}

Result<Void> FileDescriptor::set_socket_option(int level, int optname,
                                               const void *optval,
                                               socklen_t optlen) {
  if (setsockopt(_fd, level, optname, optval, optlen) < 0) {
    return ERR(Void, "Failed to set socket option");
  }
  return OKV;
}

Result<ssize_t> FileDescriptor::sock_send(const void *buf, size_t size) {
  ssize_t res = send(_fd, buf, size, 0);
  if (res < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return OK(ssize_t, 0); // Return 0 for would block
    }
    return ERR(ssize_t, "send failed");
  }
  return OK(ssize_t, res);
}

Result<std::string> FileDescriptor::read_file_line() {
  if (fp == NULL)
    return ERR(std::string, "FILE not initialized");
  std::string res;
  char *buf = new char[4097];
  while (std::fgets(buf, 4096, fp)) {
    res += buf;
    if (std::strlen(buf) < 4096)
      return (delete[] buf, OK(std::string, res));
    delete[] buf;
    buf = new char[4097];
  }
  if (!std::feof(fp))
    return ERR(std::string,
               "file read failed"); // TODO: specify error kind and message
  res += buf;
  delete[] buf;
  return OK(std::string, res);
}

bool operator==(const int &lhs, const FileDescriptor &rhs) {
  return lhs == rhs._fd;
}

bool operator!=(const int &lhs, const FileDescriptor &rhs) {
  return !(lhs == rhs);
}
