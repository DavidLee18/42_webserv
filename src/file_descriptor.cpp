#include "webserv.h"

FileDescriptor::FileDescriptor(const int raw_fd) throw(
    InvalidFileDescriptorException) {
  if (raw_fd < 0)
    throw InvalidFileDescriptorException();
  _fd = raw_fd;
}

FileDescriptor FileDescriptor::socket_new() throw(
    AccessDeniedException, NotSupportedOperationException,
    InvalidOperationException, FdTooManyException, OutOfMemoryException) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    switch (errno) {
    case EACCES:
      throw AccessDeniedException();
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
      throw NotSupportedOperationException();
    case EINVAL:
      throw InvalidOperationException();
    case EMFILE:
    case ENFILE:
      throw FdTooManyException();
    case ENOBUFS:
    case ENOMEM:
      throw OutOfMemoryException();
    }
  }
  FileDescriptor fd(sock);
  return fd;
}

FileDescriptor::FileDescriptor(FileDescriptor &other) throw(
    InvalidFileDescriptorException)
    : _fd(other._fd) {
  if (other._fd < 0)
    throw InvalidFileDescriptorException();
  other._fd = -1;
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor other) throw(
    InvalidFileDescriptorException) {
  if (this != &other) {
    if (_fd < 0)
      throw InvalidFileDescriptorException();
    _fd = other._fd;
    other._fd = -1;
  }
  return *this;
}

FileDescriptor::~FileDescriptor() {
  if (_fd >= 0)
    close(_fd);
}

const int &FileDescriptor::operator*() const { return _fd; }

bool FileDescriptor::set_blocking(const bool blocking) {
  return fcntl(_fd, F_SETFL, blocking ? 0 : O_NONBLOCK) == 0;
}

void FileDescriptor::socket_bind(
    struct in_addr addr,
    unsigned short port) throw(AccessDeniedException,
                               AddressNotAvailableException,
                               InvalidFileDescriptorException,
                               InvalidOperationException, AddressFaultException,
                               AddressLoopException, NameTooLongException,
                               NotFoundException, OutOfMemoryException,
                               ReadOnlyFileSystemException) {
  sockaddr_in _addr;
  std::memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family = AF_INET;
  _addr.sin_addr = addr;
  _addr.sin_port = htons(port);
  if (bind(_fd, reinterpret_cast<const sockaddr *>(&_addr), sizeof(_addr)) <
      0) {
    switch (errno) {
    case EACCES:
      throw AccessDeniedException();
    case EADDRINUSE:
    case EADDRNOTAVAIL:
      throw AddressNotAvailableException();
    case EBADF:
    case ENOTSOCK:
      throw InvalidFileDescriptorException();
    case EINVAL:
      throw InvalidOperationException();
    case EFAULT:
      throw AddressFaultException();
    case ELOOP:
      throw AddressLoopException();
    case ENAMETOOLONG:
      throw NameTooLongException();
    case ENOENT:
    case ENOTDIR:
      throw NotFoundException();
    case ENOMEM:
      throw OutOfMemoryException();
    case EROFS:
      throw ReadOnlyFileSystemException();
    }
  }
}

void FileDescriptor::socket_listen(unsigned short backlog) throw(
    AddressNotAvailableException, InvalidFileDescriptorException,
    NotSupportedOperationException) {
  if (listen(_fd, backlog) < 0) {
    switch (errno) {
    case EADDRINUSE:
      throw AddressNotAvailableException();
    case EBADF:
    case ENOTSOCK:
      throw InvalidFileDescriptorException();
    case EOPNOTSUPP:
      throw NotSupportedOperationException();
    }
  }
}

FileDescriptor
FileDescriptor::socket_accept(struct sockaddr *addr, socklen_t *len) throw(
    TryAgainException, ConnectionAbortedException,
    InvalidFileDescriptorException, std::invalid_argument,
    AddressFaultException, InterruptedException, FdTooManyException,
    OutOfMemoryException, NotSupportedOperationException,
    AccessDeniedException) {
  int fd = accept(_fd, addr, len);
  if (fd >= 0) {
    FileDescriptor fd_(fd);
    return fd_;
  }
  switch (errno) {
  case EWOULDBLOCK:
    throw TryAgainException();
  case EBADF:
  case ENOTSOCK:
    throw InvalidFileDescriptorException();
  case ECONNABORTED:
    throw ConnectionAbortedException();
  case EFAULT:
    throw AddressFaultException();
  case EINTR:
    throw InterruptedException();
  case EINVAL:
    throw std::invalid_argument(
        "Socket is not listening for connections, or addrlen is invalid.");
  case EMFILE:
  case ENFILE:
    throw FdTooManyException();
  case ENOBUFS:
  case ENOMEM:
    throw OutOfMemoryException();
  case EOPNOTSUPP:
    throw std::invalid_argument(
        "The referenced socket is not of type SOCK_STREAM.");
  case EPERM:
    throw AccessDeniedException();
  default:
    throw std::runtime_error("an unknown error occured during accept().");
  }
}

bool operator==(const int &lhs, const FileDescriptor &rhs) {
  return lhs == rhs._fd;
}

bool operator!=(const int &lhs, const FileDescriptor &rhs) {
  return !(lhs == rhs);
}
