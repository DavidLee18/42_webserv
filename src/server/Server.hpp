#ifndef SERVER_HPP
#define SERVER_HPP

/**
 * @file Server.hpp
 * @brief Defines the main Server class that manages epoll, connections, and
 * event loops.
 */

#include "../cgi_1_1.h"
#include "../config/WebserverConfig.hpp"
#include "../epoll_kqueue.h"
#include "../errors.h"

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

class ServerConfig;

/**
 * @class Server
 * @brief Core server class to initiate, configure, and run the event loop.
 *
 * The Server class is responsible for setting up listening sockets based on the
 * configuration, managing multiplexed I/O using EPoll, and directing I/O events
 * to the respective ClientSession handlers.
 */
class Server {
  /**
   * @brief The core polling event queue instance.
   *
   * Wraps multiplexing mechanisms like epoll or kqueue.
   */
  EPoll epoll;

  WebserverConfig config; ///< Holds the fully parsed configuration for this
                          ///< webserver instance.
  std::map<std::string, std::string>
      mime_type; ///< Map containing recognized MIME types.
  std::set<const FileDescriptor *>
      server_fds; ///< Set of active server listening socket FileDescriptors.

  std::map<const FileDescriptor *,
           std::pair<const FileDescriptor *, CgiDelegate *> >
      cgis;

  /**
   * @brief Map tying server listening sockets to their specific ServerConfig
   * settings. Key: Server socket FileDescriptor. Value: Pointer to
   * corresponding ServerConfig.
   */
  std::map<const FileDescriptor *, const ServerConfig *> listeners;

  /**
   * @brief Active client sessions currently managed by the server.
   * Key: Client connection socket FileDescriptor. Value: Active ClientSession
   * info.
   */
  std::map<const FileDescriptor *, ClientSession> clients;

  /**
   * @brief Server sessions.
   * Key: Session id. Value: User info.
   */
  Session sessions;

  /**
   * @brief Accepts a newly incoming connection from a specific server listening
   * socket.
   *
   * @param server_fd The server FileDescriptor receiving the connection.
   */
  void new_connection(const FileDescriptor *server_fd);

  /**
   * @brief Gracefully handles client disconnection, cleans up session memory
   * and epoll registration.
   *
   * @param client_fd The client FileDescriptor that disconnected.
   */
  void disconnect(const FileDescriptor *client_fd);

  /**
   * @brief Handles a read event on a registered client socket (processes
   * incoming Request).
   *
   * @param client_fd The client FileDescriptor that is ready to be read.
   */
  void client_read(const FileDescriptor *client_fd, char **envp);

  /**
   * @brief Handles a write event on a registered client socket (flushes
   * outgoing Response).
   *
   * @param client_fd The client FileDescriptor that is ready to be written.
   */
  void client_write(const FileDescriptor *client_fd);

  /**
   * @brief Dispatches a complete request to either CGI or normal response
   * generation, logging and handling the output.
   *
   * @param client_fd The client socket.
   * @param client The client session state.
   * @param envp Environment variables.
   */
  void dispatch_request(const FileDescriptor *client_fd, ClientSession &client,
                        char **envp);

  /**
   * @brief Queues a response to the client output buffer, respecting
   * keep-alive and handling disconnection.
   *
   * @param client_fd The client socket.
   * @param response The HTTP response to queue.
   */
  void queue_response(const FileDescriptor *client_fd, const Response &response);

  void reap_cgi(CgiDelegate *cgi);

public:
  /**
   * @brief Constructs a new Server based on the parsed WebserverConfig.
   *
   * @param config Reference to the populated WebserverConfig object.
   */
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
  /**
   * @brief Initializes server state, binding sockets and registering them to
   * the polling queue.
   *
   * @return Result<Void> Success or mapped error describing failure during
   * initialization.
   */
  Result<Void> init();

  /**
   * @brief Enters the main event loop, actively awaiting and processing network
   * I/O.
   *
   * @return Result<Void> Success or mapped error on loop failure.
   */
  Result<Void> start(char **envp);
};

#endif
