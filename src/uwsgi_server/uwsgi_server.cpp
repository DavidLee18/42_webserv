#include "uwsgi_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
        std::cerr << "socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    int optval = 1;
    setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
               static_cast<socklen_t>(sizeof(optval)));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<unsigned short>(_port));

    if (bind(_server_fd, reinterpret_cast<struct sockaddr *>(&addr),
             sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    if (listen(_server_fd, 128) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << std::endl;
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
        socklen_t          client_len = sizeof(client_addr);
        int client_fd = accept(_server_fd,
                               reinterpret_cast<struct sockaddr *>(&client_addr),
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            std::cerr << "accept() failed: " << std::strerror(errno)
                      << std::endl;
            continue;
        }

        handle_connection(client_fd);
        close(client_fd);
    }
}

bool UwsgiServer::read_all(int fd, void *buf, size_t len) {
    size_t total = 0;
    char  *p     = reinterpret_cast<char *>(buf);
    while (total < len) {
        ssize_t n = read(fd, p + total, len - total);
        if (n <= 0)
            return false;
        total += static_cast<size_t>(n);
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
        std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
        return "";
    }
    if (pipe(stdout_pipe) < 0) {
        std::cerr << "pipe() failed: " << std::strerror(errno) << std::endl;
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return "";
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed: " << std::strerror(errno) << std::endl;
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return "";
    }

    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0)
            _exit(1);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0)
            _exit(1);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // Build environment from uwsgi vars
        std::vector<std::string> env_strings;
        for (std::map<std::string, std::string>::const_iterator it =
                 vars.begin();
             it != vars.end(); ++it) {
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
            ssize_t n = write(stdin_pipe[1], body.c_str() + written,
                              body.size() - written);
            if (n <= 0)
                break;
            written += static_cast<size_t>(n);
        }
    }
    close(stdin_pipe[1]);

    // Read HTTP response from child's stdout
    std::string response;
    char        buf[4096];
    while (true) {
        ssize_t n = read(stdout_pipe[0], buf, sizeof(buf));
        if (n <= 0)
            break;
        response.append(buf, static_cast<size_t>(n));
    }
    close(stdout_pipe[0]);

    int wstatus;
    waitpid(pid, &wstatus, 0);

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
    size_t            sent     = 0;
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

    unsigned char  modifier1 = header[0];
    unsigned short datasize  = static_cast<unsigned short>(
        static_cast<unsigned int>(header[1]) |
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

    // Read request body if CONTENT_LENGTH is provided
    std::string body;
    std::map<std::string, std::string>::const_iterator cl_it =
        vars.find("CONTENT_LENGTH");
    if (cl_it != vars.end() && !cl_it->second.empty()) {
        long content_length = std::atol(cl_it->second.c_str());
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
        ssize_t n = write(client_fd, response.c_str() + sent,
                          response.size() - sent);
        if (n <= 0)
            break;
        sent += static_cast<size_t>(n);
    }
}
