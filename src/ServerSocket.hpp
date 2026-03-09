#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include "WebserverConfig.hpp"
#include "epoll_kqueue.h"
#include "errors.h"
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

Result<EPoll> init_servers(const WebserverConfig &config,
                           std::set<const FileDescriptor *> &server_fds);
void run_server(EPoll &epoll,
                const std::set<const FileDescriptor *> &server_fds);

#endif