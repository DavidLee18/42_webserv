#include "webserv.h"

volatile sig_atomic_t sig = 0;

// Structure to hold client connection state
struct ClientConnection {
  intptr_t fd_key; // Pointer-based key for tracking
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;

  explicit ClientConnection(intptr_t key)
      : fd_key(key), read_buffer(), write_buffer(), request_complete(false) {}

  // Copy constructor
  ClientConnection(const ClientConnection &other)
      : fd_key(other.fd_key), read_buffer(other.read_buffer),
        write_buffer(other.write_buffer),
        request_complete(other.request_complete) {}

  // Assignment operator
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

// Generate a simple HTTP response
std::string generate_response(const Http::Request &request) {
  (void)request; // Suppress unused warning for now

  // Simple 200 OK response
  std::ostringstream response;
  response << "HTTP/1.1 200 OK\r\n";
  response << "Content-Type: text/html\r\n";
  std::string body = "<html><body><h1>Hello from webserv!</h1></body></html>";
  response << "Content-Length: " << body.length() << "\r\n";
  response << "Connection: close\r\n";
  response << "\r\n";
  response << body;

  return response.str();
}

int main(const int argc, char *argv[]) {
  (void)argc;
  // if (argc != 3) {
  //   std::cerr << "Usage: webserv <config_file>" << std::endl;
  //   return 1;
  // }
  // Result<std::pair<Json *, size_t> > res = Json::Parser::parse(argv[2], '\0');
  // PANIC(res)
  // Json *js = res.value()->first;
  // std::cout << *js << std::endl;
  // delete js;
  Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1]);
  Result<WebserverConfig> config = WebserverConfig::parse(const_cast<FileDescriptor&>(fd.value()));

  signal(SIGINT, wrap_up);

#ifdef __APPLE__
  KQueue k_queue(1000);
  std::cerr << "KQueue not fully implemented yet" << std::endl;
  return 1;
#else
  // Create EPoll instance
  Result<EPoll> rep = EPoll::create(1000);
  if (!rep.error().empty()) {
    std::cerr << "EPoll::create failed: " << rep.error() << std::endl;
    return 1;
  }
  EPoll ep = rep.value();
  std::cout << "EPoll created successfully" << std::endl;

  // For now, bind to a single port (8080) for testing
  // TODO: Extract ports from config
  std::vector<FileDescriptor *>
      listen_sockets; // Pointers to listen sockets in EPoll
  std::vector<unsigned short> ports;
  ports.push_back(8080);

  // Create and bind listening sockets
  for (size_t i = 0; i < ports.size(); ++i) {
    std::cout << "Creating socket for port " << ports[i] << "..." << std::endl;
    Result<FileDescriptor> rsock = FileDescriptor::socket_new();
    if (!rsock.error().empty()) {
      std::cerr << "socket_new failed: " << rsock.error() << std::endl;
      return 1;
    }
    FileDescriptor sock = rsock.value();
    std::cout << "Socket created" << std::endl;

    // Set socket to non-blocking mode
    // REQUIRED for edge-triggered EPoll: ET mode only notifies once per state
    // change, so we must accept/read in a loop until EAGAIN. Without
    // non-blocking mode, operations could block indefinitely, freezing the
    // event loop.
    Result<Void> rnonblock = sock.set_nonblocking();
    if (!rnonblock.error().empty()) {
      std::cerr << "Failed to set socket non-blocking: " << rnonblock.error()
                << std::endl;
      return 1;
    }

    // Enable SO_REUSEADDR
    int opt = 1;
    Result<Void> rsockopt =
        sock.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (!rsockopt.error().empty()) {
      std::cerr << "Failed to set SO_REUSEADDR: " << rsockopt.error()
                << std::endl;
      return 1;
    }

    // Bind to localhost on specified port
    struct in_addr addr;
    addr.s_addr = htonl(INADDR_ANY);
    std::cout << "Binding socket to port " << ports[i] << "..." << std::endl;
    Result<Void> rbind = sock.socket_bind(addr, ports[i]);
    if (!rbind.error().empty()) {
      std::cerr << "socket_bind failed: " << rbind.error() << std::endl;
      return 1;
    }
    std::cout << "Socket bound successfully" << std::endl;

    // Listen for connections
    std::cout << "Setting socket to listen..." << std::endl;
    Result<Void> rlisten = sock.socket_listen(128);
    if (!rlisten.error().empty()) {
      std::cerr << "socket_listen failed: " << rlisten.error() << std::endl;
      return 1;
    }
    std::cout << "Socket listening" << std::endl;

    // Add to epoll for monitoring
    std::cout << "Adding socket to epoll..." << std::endl;
    Event listen_event(NULL, true, false, false, false, false, false);
    // Edge-triggered mode (et=true): Only notifies once per state change.
    // This requires non-blocking sockets and draining all data in loops.
    Option opt_flags(true, false, false, false); // edge-triggered
    Result<const FileDescriptor *> radd =
        ep.add_fd(sock, listen_event, opt_flags);
    if (!radd.error().empty()) {
      std::cerr << "ep.add_fd failed: " << radd.error() << std::endl;
      return 1;
    }
    std::cout << "Socket added to epoll" << std::endl;

    // Store pointer to the socket (which is now in EPoll's _events vector)
    // Cast away const since we'll need to call non-const methods later
    listen_sockets.push_back(const_cast<FileDescriptor *>(radd.value()));
    std::cout << "Listening on port " << ports[i] << std::endl;
  }

  // Map to track client connections by pointer value
  std::map<intptr_t, ClientConnection *> clients;

  // Main event loop
  std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
  while (sig == 0) {
    Result<Events> w = ep.wait(100); // 100ms timeout
    PANIC(w)

    for (Events evs = w.value(); !evs.is_end();) {
      Result<const Event *> re = *evs;
      PANIC(re)
      const Event *const ev = re.value();

      // Check if this is a listening socket
      bool is_listen_socket = false;
      for (size_t i = 0; i < listen_sockets.size(); ++i) {
        if (ev->fd && ev->fd == listen_sockets[i]) {
          is_listen_socket = true;

          // Accept all pending connections (edge-triggered mode)
          // ET mode only notifies once, so we must drain the accept queue in a
          // loop. This is why non-blocking mode is essential - otherwise
          // accept() could block.
          while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            Result<FileDescriptor> rclient = listen_sockets[i]->socket_accept(
                reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);

            if (!rclient.error().empty()) {
              if (rclient.error() != Errors::try_again) {
                std::cerr << "Accept failed: " << rclient.error() << std::endl;
              }
              break;
            }

            FileDescriptor client_fd = rclient.value();

            // Set client socket to non-blocking
            // REQUIRED for edge-triggered EPoll: Allows recv() to return EAGAIN
            // when no data is available, preventing the event loop from
            // blocking.
            Result<Void> rnonblock = client_fd.set_nonblocking();
            if (!rnonblock.error().empty()) {
              std::cerr << "Failed to set client socket non-blocking: "
                        << rnonblock.error() << std::endl;
              continue;
            }

            // Add client to epoll
            Event client_event(NULL, true, false, true, false, false, false);
            // Edge-triggered mode: Must drain all data in loops until EAGAIN
            Option client_opt(true, false, false, false); // edge-triggered
            Result<const FileDescriptor *> radd_client =
                ep.add_fd(client_fd, client_event, client_opt);
            if (!radd_client.error().empty()) {
              std::cerr << "Failed to add client to epoll: "
                        << radd_client.error() << std::endl;
              continue;
            }

            // Get pointer to the client FileDescriptor in EPoll's vector
            // Cast away const since we'll need to call non-const methods later
            FileDescriptor *client_fdptr =
                const_cast<FileDescriptor *>(radd_client.value());
            intptr_t client_fd_val =
                reinterpret_cast<intptr_t>(client_fdptr); // Use pointer as key
            clients[client_fd_val] = new ClientConnection(client_fd_val);

            std::cout << "Accepted new client connection" << std::endl;
          }
          break;
        }
      }

      if (!is_listen_socket && ev->fd) {
        // Handle client socket events
        intptr_t client_fd_val =
            reinterpret_cast<intptr_t>(ev->fd); // Use pointer as key
        std::map<intptr_t, ClientConnection *>::iterator it =
            clients.find(client_fd_val);

        if (it != clients.end()) {
          ClientConnection *client = it->second;
          FileDescriptor *client_fdptr = const_cast<FileDescriptor *>(ev->fd);
          bool client_deleted = false;

          // Handle read event
          if (ev->in) {
            bool client_closed = false;
            bool read_error = false;

            // Read all available data (edge-triggered mode)
            // ET only notifies once per state change, so we must drain all
            // data. Non-blocking mode allows recv() to return EAGAIN when no
            // more data is ready.
            while (true) {
              char buffer[4096];
              Result<ssize_t> rread =
                  client_fdptr->sock_recv(buffer, sizeof(buffer) - 1);

              if (!rread.error().empty()) {
                if (rread.error() != Errors::try_again) {
                  std::cerr << "Read error: " << rread.error() << std::endl;
                  read_error = true;
                }
                break;
              }

              ssize_t bytes_read = rread.value();
              if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                client->read_buffer.append(buffer,
                                           static_cast<size_t>(bytes_read));
              } else if (bytes_read == 0) {
                // Client closed connection
                std::cout << "Client disconnected" << std::endl;
                client_closed = true;
                break;
              }
            }

            if (client_closed || read_error) {
              ep.del_fd(*client_fdptr);
              delete client;
              clients.erase(it);
              client_deleted = true;
            } else if (!client->read_buffer.empty()) {
              // Try to parse HTTP request
              Result<std::pair<Http::Request, size_t> > rreq =
                  Http::Request::Parser::parse(client->read_buffer.c_str(),
                                               client->read_buffer.length());

              if (!rreq.error().empty()) {
                // Request not complete yet or parse error
                if (client->read_buffer.length() > 100000) {
                  // Request too large
                  std::cerr << "Request too large" << std::endl;
                  ep.del_fd(*client_fdptr);
                  delete client;
                  clients.erase(it);
                  client_deleted = true;
                }
              } else {
                // Request parsed successfully
                Http::Request request = rreq.value().first;
                client->request_complete = true;

                std::cout << "Received request: " << request.method() << " "
                          << request.path() << std::endl;

                // Generate response
                client->write_buffer = generate_response(request);

                // Modify epoll to monitor for write readiness
                Event write_event(NULL, false, true, true, false, false, false);
                Option write_opt(true, false, false, false);
                Result<Void> rmod =
                    ep.modify_fd(*client_fdptr, write_event, write_opt);
                if (!rmod.error().empty()) {
                  std::cerr
                      << "Failed to modify epoll for writing: " << rmod.error()
                      << std::endl;
                }
              }
            }
          }

          // Handle write event
          if (!client_deleted && ev->out && !client->write_buffer.empty()) {
            Result<ssize_t> rsend = client_fdptr->sock_send(
                client->write_buffer.c_str(), client->write_buffer.length());

            if (!rsend.error().empty()) {
              std::cerr << "Send error: " << rsend.error() << std::endl;
              ep.del_fd(*client_fdptr);
              delete client;
              clients.erase(it);
              client_deleted = true;
            } else {
              ssize_t bytes_sent = rsend.value();
              if (bytes_sent > 0) {
                client->write_buffer.erase(0, static_cast<size_t>(bytes_sent));

                if (client->write_buffer.empty()) {
                  // Response sent completely, close connection
                  std::cout << "Response sent, closing connection" << std::endl;
                  ep.del_fd(*client_fdptr);
                  delete client;
                  clients.erase(it);
                  client_deleted = true;
                }
              }
            }
          }

          // Handle errors
          if (!client_deleted && (ev->err || ev->hup)) {
            std::cout << "Client error or hangup" << std::endl;
            ep.del_fd(*client_fdptr);
            delete client;
            clients.erase(it);
          }
        }
      }

      Result<Void> rinc = ++evs;
      PANIC(rinc)
    }
  }

  // Clean up remaining clients
  for (std::map<intptr_t, ClientConnection *>::iterator it = clients.begin();
       it != clients.end(); ++it) {
    delete it->second;
  }
  clients.clear();

  std::cout << "\nShutting down server..." << std::endl;
#endif
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
