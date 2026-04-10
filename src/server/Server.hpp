#ifndef SERVER_HPP
#define SERVER_HPP

/**
 * @file Server.hpp
 * @brief Defines the main Server class that manages epoll, connections, and event loops.
 */

#include "../config/WebserverConfig.hpp"
#include "../epoll_kqueue.h"
#include "../errors.h"

#include "Client.hpp"
#include "Response.hpp"
#include "Session.hpp"

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

/**
 * @class Server
 * @brief Core server class to initiate, configure, and run the event loop.
 * 
 * The Server class is responsible for setting up listening sockets based on the configuration,
 * managing multiplexed I/O using EPoll (or Kqueue wrappers), and directing I/O events to
 * the respective ClientSession handlers.
 */
class Server {
private:
  /**
   * @brief The core polling event queue instance.
   * 
   * Wraps multiplexing mechanisms like epoll or kqueue.
   */
  EPoll epoll;

  WebserverConfig config; ///< Holds the fully parsed configuration for this webserver instance.
  std::map<std::string, std::string> mime_type; ///< Map containing recognized MIME types.
  std::set<const FileDescriptor *> server_fds;  ///< Set of active server listening socket FileDescriptors.

  // Session session; ///< Session.

  /**
   * @brief Map tying server listening sockets to their specific ServerConfig settings.
   * Key: Server socket FileDescriptor. Value: Pointer to corresponding ServerConfig.
   */
  std::map<const FileDescriptor *, const ServerConfig *> listeners;

  /**
   * @brief Active client sessions currently managed by the server.
   * Key: Client connection socket FileDescriptor. Value: Active ClientSession info.
   */
  std::map<const FileDescriptor *, ClientSession> clients;

  /**
   * @brief Server sessions.
   * Key: Session id. Value: User info.
   */
  std::map<std::string, std::string> sessions;

  /**
   * @brief Accepts a newly incoming connection from a specific server listening socket.
   * 
   * @param server_fd The server FileDescriptor receiving the connection.
   */
  void new_connection(const FileDescriptor *server_fd);

  /**
   * @brief Gracefully handles client disconnection, cleans up session memory and epoll registration.
   * 
   * @param client_fd The client FileDescriptor that disconnected.
   */
  void disconnect(const FileDescriptor *client_fd);

  /**
   * @brief Handles a read event on a registered client socket (processes incoming Request).
   * 
   * @param client_fd The client FileDescriptor that is ready to be read.
   */
  void client_read(const FileDescriptor *client_fd);

  /**
   * @brief Handles a write event on a registered client socket (flushes outgoing Response).
   * 
   * @param client_fd The client FileDescriptor that is ready to be written.
   */
  void client_write(const FileDescriptor *client_fd);

public:
  /**
   * @brief Constructs a new Server based on the parsed WebserverConfig.
   * 
   * @param config Reference to the populated WebserverConfig object.
   */
  Server(const WebserverConfig &config)
      : config(config), mime_type(config.get_type_map()) {
    mime_type["default"] = config.get_default_mime();
  };

  /**
   * @brief Destroys the Server object.
   */
  ~Server(){};

  /**
   * @brief Initializes server state, binding sockets and registering them to the polling queue.
   * 
   * @return Result<Void> Success or mapped error describing failure during initialization.
   */
  Result<Void> init();

  /**
   * @brief Enters the main event loop, actively awaiting and processing network I/O.
   * 
   * @return Result<Void> Success or mapped error on loop failure.
   */
  Result<Void> start();
};

#endif