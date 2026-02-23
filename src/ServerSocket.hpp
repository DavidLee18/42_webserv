#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include "webserv.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <set>
#include <map>

// Class holding client connection state
class ClientConnection
{
public:
  FileDescriptor *fd_ptr; // Non-const pointer for explicit close() calls
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  explicit ClientConnection(FileDescriptor *ptr)
      : fd_ptr(ptr), read_buffer(), write_buffer(), request_complete(false) {}

  ClientConnection(const ClientConnection &other)
      : fd_ptr(other.fd_ptr),
        read_buffer(other.read_buffer),
        write_buffer(other.write_buffer),
        request_complete(other.request_complete) {}

  ClientConnection &operator=(const ClientConnection &other)
  {
    if (this != &other)
    {
      fd_ptr = other.fd_ptr;
      read_buffer = other.read_buffer;
      write_buffer = other.write_buffer;
      request_complete = other.request_complete;
    }
    return *this;
  }
};

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds);
void run_server(EPoll &epoll, const std::set<const FileDescriptor *> &server_fds);

#endif