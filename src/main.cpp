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
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1]);
  if (!fd.error().empty()) {
    std::cerr << "file open failed: " << fd.error() << std::endl;
    return 1;
  }
  Result<WebserverConfig> result_config = WebserverConfig::parse(fd.value_mut());
  if (!result_config.error().empty()) {
    std::cerr << "config parsing failed: " << result_config.error() << std::endl;
    return 1;
  }
  else {
    WebserverConfig config = result_config.value();
    // std::cout << a << std::endl;
  }
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
