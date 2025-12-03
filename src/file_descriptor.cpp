#include "webserv.h"

Result<FileDescriptor> FileDescriptor::socket_new() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    switch (errno) {
    case EACCES:
      return ERR(FileDescriptor, errors::access_denied);
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
      return ERR(FileDescriptor, errors::not_supported);
    case EINVAL:
      return ERR(FileDescriptor, errors::invalid_operation);
    case EMFILE:
    case ENFILE:
      return ERR(FileDescriptor, errors::fd_too_many);
    case ENOBUFS:
    case ENOMEM:
      return ERR(FileDescriptor, errors::out_of_mem);
    }
  }
  FileDescriptor *fd = new FileDescriptor();
  fd->set_fd(sock);
  return OK(FileDescriptor, fd);
}

Result<FileDescriptor> FileDescriptor::from_raw(int raw_fd) {
  if (raw_fd < 0)
    return ERR(FileDescriptor, errors::invalid_fd);
  FileDescriptor *fd = new FileDescriptor();
  fd->set_fd(raw_fd);
  return OK(FileDescriptor, fd);
}

Result<FileDescriptor> FileDescriptor::move_from(FileDescriptor other) {
  if (other._fd < 0)
    return ERR(FileDescriptor, errors::invalid_fd);
  FileDescriptor *fd = new FileDescriptor();
  fd->set_fd(other._fd);
  return OK(FileDescriptor, fd);
}

Result<Void> FileDescriptor::operator=(FileDescriptor other) {
  if (this != &other) {
    if (_fd < 0)
      return ERR(Void, errors::invalid_fd);
    _fd = other._fd;
    other._fd = -1;
  }
  return OK(Void, void_);
}

FileDescriptor::~FileDescriptor() {
  if (_fd >= 0)
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
      return ERR(Void, errors::access_denied);
    case EADDRINUSE:
    case EADDRNOTAVAIL:
      return ERR(Void, errors::addr_not_available);
    case EBADF:
    case ENOTSOCK:
      return ERR(Void, errors::invalid_fd);
    case EINVAL:
      return ERR(Void, errors::invalid_operation);
    case EFAULT:
      return ERR(Void, errors::address_fault);
    case ELOOP:
      return ERR(Void, errors::addr_loop);
    case ENAMETOOLONG:
      return ERR(Void, errors::name_too_long);
    case ENOENT:
    case ENOTDIR:
      return ERR(Void, errors::not_found);
    case ENOMEM:
      return ERR(Void, errors::out_of_mem);
    case EROFS:
      return ERR(Void, errors::readonly_filesys);
    }
  }
  return OK(Void, new Void);
}

Result<Void> FileDescriptor::socket_listen(unsigned short backlog) {
  if (listen(_fd, backlog) < 0) {
    switch (errno) {
    case EADDRINUSE:
      return ERR(Void, errors::addr_not_available);
    case EBADF:
    case ENOTSOCK:
      return ERR(Void, errors::invalid_fd);
    case EOPNOTSUPP:
      return ERR(Void, errors::not_supported);
    }
  }
  return OK(Void, new Void);
}

Result<FileDescriptor> FileDescriptor::socket_accept(struct sockaddr *addr,
                                                     socklen_t *len) {
  int fd = accept(_fd, addr, len);
  if (fd >= 0) {
    FileDescriptor *fd_ = new FileDescriptor();
    fd_->set_fd(fd);
    return OK(FileDescriptor, fd_);
  }
  switch (errno) {
  case EWOULDBLOCK:
    return ERR(FileDescriptor, errors::try_again);
  case EBADF:
  case ENOTSOCK:
    return ERR(FileDescriptor, errors::invalid_fd);
  case ECONNABORTED:
    return ERR(FileDescriptor, errors::conn_aborted);
  case EFAULT:
    return ERR(FileDescriptor, errors::address_fault);
  case EINTR:
    return ERR(FileDescriptor, errors::interrupted);
  case EINVAL:
    return ERR(
        FileDescriptor,
        "Socket is not listening for connections, or addrlen is invalid.");
  case EMFILE:
  case ENFILE:
    return ERR(FileDescriptor, errors::fd_too_many);
  case ENOBUFS:
  case ENOMEM:
    return ERR(FileDescriptor, errors::out_of_mem);
  case EOPNOTSUPP:
    return ERR(FileDescriptor,
               "The referenced socket is not of type SOCK_STREAM.");
  case EPERM:
    return ERR(FileDescriptor, errors::access_denied);
  default:
    return ERR(FileDescriptor, "an unknown error occured during accept().");
  }
}

bool operator==(const int &lhs, const FileDescriptor &rhs) {
  return lhs == rhs._fd;
}

bool operator!=(const int &lhs, const FileDescriptor &rhs) {
  return !(lhs == rhs);
}
