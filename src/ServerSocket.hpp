#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include "epoll_kqueue.h"
#include "WebserverConfig.hpp"
#include "errors.h"
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
  int fd; // Raw fd for stable key tracking (no dangling pointer risk)
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  explicit ClientConnection(int fd)
      : fd(fd), read_buffer(), write_buffer(), request_complete(false) {}

  ClientConnection(const ClientConnection &other)
      : fd(other.fd),
        read_buffer(other.read_buffer),
        write_buffer(other.write_buffer),
        request_complete(other.request_complete) {}

  ClientConnection &operator=(const ClientConnection &other)
  {
    if (this != &other)
    {
      fd = other.fd;
      read_buffer = other.read_buffer;
      write_buffer = other.write_buffer;
      request_complete = other.request_complete;
    }
    return *this;
  }
};

Result<EPoll> init_servers(const WebserverConfig &config, std::set<int> &server_fds);
void run_server(EPoll &epoll, const std::set<int> &server_fds);

#endif