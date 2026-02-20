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

// Structure to hold client connection state
class ClientConnection {
public:
  intptr_t fd_key; // Pointer-based key for tracking
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  explicit ClientConnection(intptr_t key)
      : fd_key(key), read_buffer(), write_buffer(), request_complete(false) {}

  ClientConnection(const ClientConnection &other)
      : fd_key(other.fd_key), read_buffer(other.read_buffer),
        write_buffer(other.write_buffer),
        request_complete(other.request_complete) {}

  ClientConnection &operator=(const ClientConnection &other) {
    if (this != &other) {
      fd_key = other.fd_key;
      read_buffer = other.read_buffer;
      write_buffer = other.write_buffer;
      request_complete = other.request_complete;
    }
    return *this;
  }
};

Result<EPoll> init_servers(const WebserverConfig& config, std::set<const FileDescriptor*>& server_fds);
void run_server(EPoll& epoll, const std::set<const FileDescriptor*>& server_fds);

#endif