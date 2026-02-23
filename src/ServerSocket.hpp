#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include "epoll_kqueue.h"
#include "WebserverConfig.hpp"
#include "errors.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <cstring>
#include <set>
#include <map>
#include <string>

// Class holding client connection state
class ClientConnection
{
public:
  const FileDescriptor *fd_ptr; // Pointer-based key for tracking
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  explicit ClientConnection(const FileDescriptor *ptr)
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