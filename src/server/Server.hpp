#ifndef SERVER_HPP
#define SERVER_HPP

#include "../cgi_1_1/CgiDelegate.hpp"
#include "../config/WebserverConfig.hpp"
#include "../core/EPoll.hpp"
#include "../core/Errors.hpp"

#include "Client.hpp"
#include "Response.hpp"
#include "Session.hpp"

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

#include "../core/Result.hpp"

#define IDLE_TIMEOUT 300
#define NETWORK_BUFFER_SIZE 4096
#define CHUNKED_PENDING_TIMEOUT 3

class ServerConfig;

extern volatile sig_atomic_t g_receivedSignal;

class Server {
  EPoll epoll;

  WebserverConfig config; ///< Holds the fully parsed configuration
  std::map<std::string, std::string> mime_type; ///< Map containing MIME types.
  std::set<const FileDescriptor *> server_fds;  ///< Set of server fds.

    std::map<const FileDescriptor*,
             std::pair<const FileDescriptor*, CgiDelegate*> >
                                                         cgis;

  std::map<const FileDescriptor *, const ServerConfig *> listeners;
  std::map<const FileDescriptor *, ClientSession> clients;
  Session sessions;

  Result<Void> new_connection(const FileDescriptor *server_fd);
  Result<Void> disconnect(const FileDescriptor *client_fd);
  Result<Void> client_read(const FileDescriptor *client_fd, char **envp);
  Result<Void> client_write(const FileDescriptor *client_fd);
  Result<Void> dispatch_request(const FileDescriptor *client_fd,
                                ClientSession &client, char **envp);
  Result<Void> queue_response(const FileDescriptor *client_fd,
                              const Response &response);
  Result<Void> reap_cgi(CgiDelegate *cgi);

public:
  explicit Server(const WebserverConfig &config)
      : config(config), mime_type(config.get_type_map()) {
    mime_type["default"] = config.get_default_mime();
  }

  ~Server() {
    std::set<CgiDelegate *> cgi_set;
    for (std::map<FileDescriptor const *,
                  std::pair<FileDescriptor const *,
                            CgiDelegate *> >::const_iterator it = cgis.begin();
         it != cgis.end(); ++it)
      cgi_set.insert(it->second.second);
    for (std::set<CgiDelegate *>::iterator it = cgi_set.begin();
         it != cgi_set.end(); ++it)
      delete *it;
  }
  Result<Void> init();
  Result<Void> start(char **envp);
};

#endif
