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
#include <set>
#include <map>
#include <string>
#include <utility>

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds);
void run_server(EPoll &epoll, const std::set<const FileDescriptor *> &server_fds);

#endif