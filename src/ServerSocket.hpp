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
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  ClientConnection()
      : read_buffer(), write_buffer(), request_complete(false) {}

  ClientConnection(const ClientConnection &other)
      : read_buffer(other.read_buffer),
        write_buffer(other.write_buffer),
        request_complete(other.request_complete) {}

  ClientConnection &operator=(const ClientConnection &other)
  {
    if (this != &other)
    {
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