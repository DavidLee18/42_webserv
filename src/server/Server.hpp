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

class Server
{
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

	void handle_new_connection(const FileDescriptor *server_fd);
	void disconnect(const FileDescriptor *client_fd);
	void handle_client_read(const FileDescriptor *client_fd);
	void handle_client_write(const FileDescriptor *client_fd);

public:
	Server(const WebserverConfig &config) : config(config) {};
	~Server() {};

	Result<Void> init();
	Result<Void> start();
};

#endif