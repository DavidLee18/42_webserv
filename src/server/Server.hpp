#ifndef SERVER_HPP
#define SERVER_HPP

#include "../WebserverConfig.hpp"
#include "../epoll_kqueue.h"
#include "../errors.h"

#include "Response.hpp"

#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

class ServerConfig;
class Server {
private:
  EPoll epoll;
  WebserverConfig config;
  std::set<const FileDescriptor *> server_fds;
  // Listening socket
  // key: server socket fds, value: ports ServerConfig
  std::map<const FileDescriptor *, const ServerConfig *> listeners;
  // Manage client sessions
  // key: client fds, value: session info
  std::map<const FileDescriptor *, ClientSession> clients;

  void new_connection(const FileDescriptor *server_fd);
  void disconnect(const FileDescriptor *client_fd);
  void client_read(const FileDescriptor *client_fd);
  void client_write(const FileDescriptor *client_fd);

public:
  Server(const WebserverConfig &config) : config(config){};
  ~Server(){};

  Result<Void> init();
  Result<Void> start();
};

#endif