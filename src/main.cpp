#include "webserv.h"

volatile sig_atomic_t sig = 0;

// Structure to hold client connection state
struct ClientConnection {
  int fd_value;  // Just store the fd value, not the FileDescriptor
  std::string read_buffer;
  std::string write_buffer;
  bool request_complete;
  
  explicit ClientConnection(int client_fd) 
    : fd_value(client_fd), read_buffer(), write_buffer(), request_complete(false) {}
  
  // Copy constructor
  ClientConnection(const ClientConnection &other)
    : fd_value(other.fd_value), read_buffer(other.read_buffer), write_buffer(other.write_buffer),
      request_complete(other.request_complete) {}
  
  // Assignment operator
  ClientConnection& operator=(const ClientConnection &other) {
    if (this != &other) {
      fd_value = other.fd_value;
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
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  
  // Parse configuration
  WebserverConfig config(argv[1]);
  
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
  std::vector<const FileDescriptor *> listen_sockets;  // Pointers to listen sockets in EPoll
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
    std::cout << "Socket created, fd=" << sock.get_fd() << std::endl;
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock.get_fd(), F_GETFL, 0);
    if (flags < 0 || fcntl(sock.get_fd(), F_SETFL, flags | O_NONBLOCK) < 0) {
      std::cerr << "Failed to set socket non-blocking" << std::endl;
      return 1;
    }
    
    // Enable SO_REUSEADDR
    int opt = 1;
    if (setsockopt(sock.get_fd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
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
    Option opt_flags(true, false, false, false);  // edge-triggered
    Result<const FileDescriptor *> radd = ep.add_fd(sock, listen_event, opt_flags);
    if (!radd.error().empty()) {
      std::cerr << "ep.add_fd failed: " << radd.error() << std::endl;
      return 1;
    }
    std::cout << "Socket added to epoll" << std::endl;
    
    // Store pointer to the socket (which is now in EPoll's _events vector)
    listen_sockets.push_back(radd.value());
    std::cout << "Listening on port " << ports[i] << std::endl;
  }
  
  // Map to track client connections by file descriptor value
  std::map<int, ClientConnection *> clients;
  
  // Main event loop
  std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
  while (sig == 0) {
    Result<Events> w = ep.wait(100);  // 100ms timeout
    PANIC(w)
    
    for (Events evs = w.value(); !evs.is_end(); ) {
      Result<const Event *> re = *evs;
      PANIC(re)
      const Event *const ev = re.value();
      
      // Check if this is a listening socket
      bool is_listen_socket = false;
      for (size_t i = 0; i < listen_sockets.size(); ++i) {
        if (ev->fd && ev->fd == listen_sockets[i]) {
          is_listen_socket = true;
          
          // Accept all pending connections (edge-triggered mode)
          while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            FileDescriptor *listen_sock = const_cast<FileDescriptor *>(listen_sockets[i]);
            Result<FileDescriptor> rclient = listen_sock->socket_accept(
              reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
            
            if (!rclient.error().empty()) {
              if (rclient.error() != Errors::try_again) {
                std::cerr << "Accept failed: " << rclient.error() << std::endl;
              }
              break;
            }
            
            FileDescriptor client_fd = rclient.value();
            int client_fd_val = client_fd.get_fd();  // Get fd value before moving
            
            // Set client socket to non-blocking
            int flags = fcntl(client_fd_val, F_GETFL, 0);
            if (flags < 0 || fcntl(client_fd_val, F_SETFL, flags | O_NONBLOCK) < 0) {
              std::cerr << "Failed to set client socket non-blocking" << std::endl;
              continue;
            }
            
            // Add client to epoll
            Event client_event(NULL, true, false, true, false, false, false);
            Option client_opt(true, false, false, false);  // edge-triggered
            Result<const FileDescriptor *> radd_client = ep.add_fd(client_fd, client_event, client_opt);
            if (!radd_client.error().empty()) {
              std::cerr << "Failed to add client to epoll: " << radd_client.error() << std::endl;
              continue;
            }
            
            // Create and store client connection
            clients[client_fd_val] = new ClientConnection(client_fd_val);
            
            std::cout << "Accepted new client connection (fd=" << client_fd_val << ")" << std::endl;
          }
          break;
        }
      }
      
      if (!is_listen_socket && ev->fd) {
        // Handle client socket events
        int client_fd_val = ev->fd->get_fd();
        std::map<int, ClientConnection *>::iterator it = clients.find(client_fd_val);
        
        if (it != clients.end()) {
          ClientConnection *client = it->second;
          FileDescriptor *client_fdptr = const_cast<FileDescriptor *>(ev->fd);
          bool client_deleted = false;
          
          // Handle read event
          if (ev->in) {
            bool client_closed = false;
            bool read_error = false;
            
            // Read all available data (edge-triggered mode)
            while (true) {
              char buffer[4096];
              Result<ssize_t> rread = client_fdptr->sock_recv(buffer, sizeof(buffer) - 1);
              
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
                client->read_buffer.append(buffer, static_cast<size_t>(bytes_read));
              } else if (bytes_read == 0) {
                // Client closed connection
                std::cout << "Client disconnected (fd=" << client_fd_val << ")" << std::endl;
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
                Http::Request::Parser::parse(client->read_buffer.c_str(), client->read_buffer.length());
              
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
                
                std::cout << "Received request: " << request.method() << " " << request.path() << std::endl;
                
                // Generate response
                client->write_buffer = generate_response(request);
                
                // Modify epoll to monitor for write readiness
                Event write_event(NULL, false, true, true, false, false, false);
                Option write_opt(true, false, false, false);
                Result<Void> rmod = ep.modify_fd(*client_fdptr, write_event, write_opt);
                if (!rmod.error().empty()) {
                  std::cerr << "Failed to modify epoll for writing: " << rmod.error() << std::endl;
                }
              }
            }
          }
          
          // Handle write event
          if (!client_deleted && ev->out && !client->write_buffer.empty()) {
            ssize_t bytes_sent = send(client->fd_value, client->write_buffer.c_str(), 
                                     client->write_buffer.length(), 0);
            
            if (bytes_sent > 0) {
              client->write_buffer.erase(0, static_cast<size_t>(bytes_sent));
              
              if (client->write_buffer.empty()) {
                // Response sent completely, close connection
                std::cout << "Response sent, closing connection (fd=" << client_fd_val << ")" << std::endl;
                ep.del_fd(*client_fdptr);
                delete client;
                clients.erase(it);
                client_deleted = true;
              }
            } else if (bytes_sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
              std::cerr << "Send error" << std::endl;
              ep.del_fd(*client_fdptr);
              delete client;
              clients.erase(it);
              client_deleted = true;
            }
          }
          
          // Handle errors
          if (!client_deleted && (ev->err || ev->hup)) {
            std::cout << "Client error or hangup (fd=" << client_fd_val << ")" << std::endl;
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
  for (std::map<int, ClientConnection *>::iterator it = clients.begin(); 
       it != clients.end(); ++it) {
    delete it->second;
  }
  clients.clear();
  
  std::cout << "\nShutting down server..." << std::endl;
#endif
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
