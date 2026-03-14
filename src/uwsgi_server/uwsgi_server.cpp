#include "uwsgi_server.h"

#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Maximum allowed request body size (10 MB)
static const long MAX_BODY_SIZE = 10 * 1024 * 1024;

// Timeout in seconds for child WSGI process
static const int CHILD_TIMEOUT_SEC = 30;

// Allowlisted WSGI/CGI environment variable names (exact match).
// Any key starting with "HTTP_" is also allowed.
// See https://peps.python.org/pep-3333/ and RFC 3875 for the full list.
static const char *const ALLOWED_VARS[] = {
    "REQUEST_METHOD",  "SCRIPT_NAME",     "PATH_INFO",         "QUERY_STRING",
    "CONTENT_TYPE",    "CONTENT_LENGTH",  "SERVER_NAME",       "SERVER_PORT",
    "SERVER_PROTOCOL", "SERVER_SOFTWARE", "GATEWAY_INTERFACE", "REMOTE_ADDR",
    "REMOTE_HOST",     "REMOTE_IDENT",    "REMOTE_USER",       NULL};

// Returns true only when key is a legitimate WSGI/CGI variable name.
// Rejects keys containing NUL or '=' to prevent environment injection
// (e.g. LD_PRELOAD, PYTHONPATH, PATH).
static bool is_allowed_wsgi_var(const std::string &key) {
  if (key.find('\0') != std::string::npos)
    return false;
  if (key.find('=') != std::string::npos)
    return false;
  // Allow HTTP_* request-header variables
  if (key.size() > 5 && key.compare(0, 5, "HTTP_") == 0)
    return true;
  for (int i = 0; ALLOWED_VARS[i] != NULL; ++i) {
    if (key == ALLOWED_VARS[i])
      return true;
  }
  return false;
}

UwsgiServer::UwsgiServer(const std::string &script_path, int port)
    : _script_path(script_path), _port(port), _server_fd(-1) {}

UwsgiServer::~UwsgiServer() {
  if (_server_fd >= 0) {
    close(_server_fd);
    _server_fd = -1;
  }
}

bool UwsgiServer::setup_socket() {
  _server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (_server_fd < 0) {
    std::cerr << "socket() failed" << std::endl;
    return false;
  }

  // Prevent the listening socket from being inherited by child processes
  if (fcntl(_server_fd, F_SETFD, FD_CLOEXEC) < 0) {
    std::cerr << "fcntl(FD_CLOEXEC) failed" << std::endl;
    close(_server_fd);
    _server_fd = -1;
    return false;
  }

  int optval = 1;
  if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                 static_cast<socklen_t>(sizeof(optval))) < 0) {
    std::cerr << "setsockopt() failed" << std::endl;
    close(_server_fd);
    _server_fd = -1;
    return false;
  }

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<unsigned short>(_port));

  if (bind(_server_fd, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) < 0) {
    std::cerr << "bind() failed" << std::endl;
    return false;
  }

  if (listen(_server_fd, 128) < 0) {
    std::cerr << "listen() failed" << std::endl;
    return false;
  }

  return true;
}

void UwsgiServer::run() {
  if (!setup_socket()) {
    return;
  }

  // Prevent broken-pipe signals from terminating the server when a client
  // closes the connection while we are still writing.
  signal(SIGPIPE, SIG_IGN);

  std::cout << "uWSGI server listening on port " << _port << std::endl;
  std::cout << "WSGI script: " << _script_path << std::endl;

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        accept(_server_fd, reinterpret_cast<struct sockaddr *>(&client_addr),
               &client_len);
    if (client_fd < 0) {
      std::cerr << "accept() failed" << std::endl;
      continue;
    }

    // Prevent the client socket from leaking into child processes
    if (fcntl(client_fd, F_SETFD, FD_CLOEXEC) < 0) {
      std::cerr << "fcntl(FD_CLOEXEC) failed" << std::endl;
      close(client_fd);
      continue;
    }

    handle_connection(client_fd);
    close(client_fd);
  }
}

bool UwsgiServer::read_all(int fd, void *buf, size_t len) {
  size_t total = 0;
  char *p = reinterpret_cast<char *>(buf);
  while (total < len) {
    ssize_t n = read(fd, p + total, len - total);
    if (n > 0) {
      total += static_cast<size_t>(n);
      continue;
    }
    // n == 0: EOF before reading all bytes; n < 0: error
    return false;
  }
  return true;
}

bool UwsgiServer::parse_uwsgi_vars(const std::vector<unsigned char> &data,
                                   std::map<std::string, std::string> &vars) {
  size_t pos = 0;
  while (pos < data.size()) {
    // Need at least 4 bytes for key_len + val_len
    if (pos + 2 > data.size())
      return false;

    unsigned short key_len = static_cast<unsigned short>(
        static_cast<unsigned int>(data[pos]) |
        (static_cast<unsigned int>(data[pos + 1]) << 8));
    pos += 2;

    if (pos + key_len > data.size())
      return false;
    std::string key(reinterpret_cast<const char *>(&data[pos]), key_len);
    pos += key_len;

    if (pos + 2 > data.size())
      return false;
    unsigned short val_len = static_cast<unsigned short>(
        static_cast<unsigned int>(data[pos]) |
        (static_cast<unsigned int>(data[pos + 1]) << 8));
    pos += 2;

    if (pos + val_len > data.size())
      return false;
    std::string val(reinterpret_cast<const char *>(&data[pos]), val_len);
    pos += val_len;

    vars[key] = val;
  }
  return true;
}

std::string
UwsgiServer::execute_wsgi(const std::map<std::string, std::string> &vars,
                          const std::string &body) {
  int stdin_pipe[2];
  int stdout_pipe[2];

  if (pipe(stdin_pipe) < 0) {
    std::cerr << "pipe() failed" << std::endl;
    return "";
  }
  if (pipe(stdout_pipe) < 0) {
    std::cerr << "pipe() failed" << std::endl;
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    return "";
  }

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "fork() failed" << std::endl;
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return "";
  }

  if (pid == 0) {
    // Child process: explicitly close the listening socket so it is not
    // held open by the Python process. FD_CLOEXEC set in setup_socket()
    // provides defense-in-depth on execve; the explicit close is direct
    // and handles any fd created before F_SETFD was applied.
    if (_server_fd >= 0)
      close(_server_fd);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);

    if (dup2(stdin_pipe[0], STDIN_FILENO) < 0)
      _exit(1);
    if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0)
      _exit(1);

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Build environment from allowlisted WSGI/CGI vars only.
    // Keys or values containing NUL bytes are silently dropped to
    // prevent environment injection (e.g. LD_PRELOAD, PYTHONPATH).
    std::vector<std::string> env_strings;
    for (std::map<std::string, std::string>::const_iterator it = vars.begin();
         it != vars.end(); ++it) {
      if (!is_allowed_wsgi_var(it->first))
        continue;
      if (it->second.find('\0') != std::string::npos)
        continue;
      env_strings.push_back(it->first + "=" + it->second);
    }

    std::vector<char *> envp;
    for (size_t i = 0; i < env_strings.size(); ++i)
      envp.push_back(const_cast<char *>(env_strings[i].c_str()));
    envp.push_back(NULL);

    char *argv[] = {const_cast<char *>("python3"),
                    const_cast<char *>(_script_path.c_str()), NULL};

    execve("/usr/bin/python3", argv, &envp[0]);
    // execve only returns on error
    _exit(1);
  }

  // Parent process
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);

  // Write request body to child's stdin
  if (!body.empty()) {
    size_t written = 0;
    while (written < body.size()) {
      ssize_t n =
          write(stdin_pipe[1], body.c_str() + written, body.size() - written);
      if (n <= 0)
        break;
      written += static_cast<size_t>(n);
    }
  }
  close(stdin_pipe[1]);

  // Read HTTP response from child's stdout
  std::string response;
  char buf[4096];
  while (true) {
    ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
    if (n <= 0)
      break;
    response.append(buf, static_cast<size_t>(n));
  }
  close(stdout_pipe[0]);

  // Wait for the child with a timeout to prevent blocking indefinitely
  // when a WSGI script hangs after producing its output.
  // Sleep in 100 ms increments up to the remaining deadline to keep
  // CPU usage low while still reaping promptly.
  int wstatus = 0;
  time_t deadline = time(NULL) + CHILD_TIMEOUT_SEC;
  while (true) {
    pid_t waited = waitpid(pid, &wstatus, WNOHANG);
    if (waited != 0)
      break;
    time_t now = time(NULL);
    if (now >= deadline) {
      kill(pid, SIGKILL);
      // Loop until the killed child is fully reaped (may be interrupted)
      while (waitpid(pid, &wstatus, 0) < 0) {
      }
      break;
    }
    // Sleep up to 100 ms, but no more than the remaining timeout
    long remaining_ms = static_cast<long>((deadline - now) * 1000L);
    long sleep_ms = (remaining_ms < 100L) ? remaining_ms : 100L;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = sleep_ms * 1000000L;
    nanosleep(&ts, NULL);
  }

  return response;
}

void UwsgiServer::send_error_response(int fd, int status,
                                      const std::string &reason) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " " << reason << "\r\n"
      << "Content-Type: text/plain\r\n"
      << "Content-Length: " << reason.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << reason;
  const std::string response = oss.str();
  size_t sent = 0;
  while (sent < response.size()) {
    ssize_t n = write(fd, response.c_str() + sent, response.size() - sent);
    if (n <= 0)
      break;
    sent += static_cast<size_t>(n);
  }
}

void UwsgiServer::handle_connection(int client_fd) {
  // Read the 4-byte uwsgi header
  unsigned char header[4];
  if (!read_all(client_fd, header, sizeof(header))) {
    send_error_response(client_fd, 400, "Bad Request");
    return;
  }

  unsigned char modifier1 = header[0];
  unsigned short datasize =
      static_cast<unsigned short>(static_cast<unsigned int>(header[1]) |
                                  (static_cast<unsigned int>(header[2]) << 8));
  // header[3] is modifier2; reserved / unused here

  // Only WSGI/Python requests (modifier1 == 0) are supported
  if (modifier1 != 0) {
    send_error_response(client_fd, 400, "Unsupported modifier");
    return;
  }

  // Read the vars block
  std::vector<unsigned char> vars_data(static_cast<size_t>(datasize));
  if (datasize > 0 &&
      !read_all(client_fd, &vars_data[0], static_cast<size_t>(datasize))) {
    send_error_response(client_fd, 400, "Failed to read vars");
    return;
  }

  // Parse key/value pairs from vars block
  std::map<std::string, std::string> vars;
  if (!parse_uwsgi_vars(vars_data, vars)) {
    send_error_response(client_fd, 400, "Failed to parse vars");
    return;
  }

  // Read request body if CONTENT_LENGTH is provided.
  // Validate that the value contains only digits and cap it at MAX_BODY_SIZE
  // to prevent allocation-based DoS attacks.
  std::string body;
  std::map<std::string, std::string>::const_iterator cl_it =
      vars.find("CONTENT_LENGTH");
  if (cl_it != vars.end() && !cl_it->second.empty()) {
    const std::string &cl_str = cl_it->second;
    char *endptr = NULL;
    long content_length = std::strtol(cl_str.c_str(), &endptr, 10);
    if (endptr == cl_str.c_str() || *endptr != '\0' || content_length < 0) {
      send_error_response(client_fd, 400, "Invalid Content-Length");
      return;
    }
    if (content_length > MAX_BODY_SIZE) {
      send_error_response(client_fd, 413, "Request Entity Too Large");
      return;
    }
    if (content_length > 0) {
      std::vector<char> body_buf(static_cast<size_t>(content_length));
      if (!read_all(client_fd, &body_buf[0],
                    static_cast<size_t>(content_length))) {
        send_error_response(client_fd, 400, "Failed to read body");
        return;
      }
      body.assign(body_buf.begin(), body_buf.end());
    }
  }

  // Execute the WSGI script and obtain its HTTP response
  const std::string response = execute_wsgi(vars, body);
  if (response.empty()) {
    send_error_response(client_fd, 500, "Internal Server Error");
    return;
  }

  // Forward the response back to the caller
  size_t sent = 0;
  while (sent < response.size()) {
    ssize_t n =
        write(client_fd, response.c_str() + sent, response.size() - sent);
    if (n <= 0)
      break;
    sent += static_cast<size_t>(n);
  }
}
