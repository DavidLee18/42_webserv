#ifndef SERVER_HPP
#define SERVER_HPP

#include "../epoll_kqueue.h"
#include "../WebserverConfig.hpp"
#include "../http_1_1.h"
#include "../errors.h"

#include "Session.hpp"
#include "Response.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <map>
#include <string>
#include <utility>

// class Server
// {
// private:
// 	ServerConfig
// 	WebserverConfig
// public:
// 	Server(/* args */);
// 	~Server();
// };

// Server::Server(/* args */)
// {
// }

// Server::~Server()
// {
// }


typedef std::map<const FileDescriptor *, const ServerConfig *> ListenerMap;

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds, ListenerMap &listener_map);
void run_server(EPoll &epoll, const std::set<const FileDescriptor *> &server_fds, const ListenerMap &listener_map);

#endif