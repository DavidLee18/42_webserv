#include "uwsgi_client.h"

#include <arpa/inet.h>
#include <climits>
#include <cstring>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

UwsgiClient::UwsgiClient(const std::string &host, int port)
    : _host(host), _port(port) {}

// Build uwsgi vars block: repeated [key_len: 2B LE][key][val_len: 2B LE][val]
static std::vector<unsigned char>
build_vars_block(const std::map<std::string, std::string> &vars) {
    std::vector<unsigned char> block;
    for (std::map<std::string, std::string>::const_iterator it = vars.begin();
         it != vars.end(); ++it) {
        const std::string &key = it->first;
        const std::string &val = it->second;

        unsigned short key_len = static_cast<unsigned short>(key.size());
        block.push_back(static_cast<unsigned char>(key_len & 0xFF));
        block.push_back(static_cast<unsigned char>((key_len >> 8) & 0xFF));
        block.insert(block.end(), key.begin(), key.end());

        unsigned short val_len = static_cast<unsigned short>(val.size());
        block.push_back(static_cast<unsigned char>(val_len & 0xFF));
        block.push_back(static_cast<unsigned char>((val_len >> 8) & 0xFF));
        block.insert(block.end(), val.begin(), val.end());
    }
    return block;
}

static bool write_all(int fd, const void *buf, size_t len) {
    const char *p = reinterpret_cast<const char *>(buf);
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, p + written, len - written);
        if (n <= 0)
            return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

Result<std::string>
UwsgiClient::send(const std::map<std::string, std::string> &vars,
                  const std::string &body) const {
    std::vector<unsigned char> vars_block = build_vars_block(vars);

    if (vars_block.size() > static_cast<size_t>(USHRT_MAX))
        return ERR(std::string, "uwsgi vars block exceeds 64 KiB limit");

    // 4-byte uwsgi header: [modifier1=0][datasize: 2B LE][modifier2=0]
    unsigned short datasize = static_cast<unsigned short>(vars_block.size());
    unsigned char header[4];
    header[0] = 0; // modifier1: WSGI/Python
    header[1] = static_cast<unsigned char>(datasize & 0xFF);
    header[2] = static_cast<unsigned char>((datasize >> 8) & 0xFF);
    header[3] = 0; // modifier2

    // Resolve and connect
    struct addrinfo hints, *res = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::ostringstream port_ss;
    port_ss << _port;
    if (getaddrinfo(_host.c_str(), port_ss.str().c_str(), &hints, &res) != 0)
        return ERR(std::string, "uwsgi: failed to resolve server address");

    int sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd < 0) {
        freeaddrinfo(res);
        return ERR(std::string, "uwsgi: failed to create socket");
    }
    if (connect(sock_fd, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(sock_fd);
        return ERR(std::string, "uwsgi: failed to connect to server");
    }
    freeaddrinfo(res);

    // Send header, vars block, and body
    if (!write_all(sock_fd, header, sizeof(header)) ||
        (!vars_block.empty() &&
         !write_all(sock_fd, &vars_block[0], vars_block.size())) ||
        (!body.empty() && !write_all(sock_fd, body.c_str(), body.size()))) {
        close(sock_fd);
        return ERR(std::string, "uwsgi: failed to send request");
    }

    // Half-close the write side so the server sees EOF on the request stream
    shutdown(sock_fd, SHUT_WR);

    // Read back the raw HTTP response until the server closes the connection
    std::string response;
    char buf[4096];
    while (true) {
        ssize_t n = read(sock_fd, buf, sizeof(buf));
        if (n > 0) {
            response.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            break; // EOF: server closed connection
        } else {
            close(sock_fd);
            return ERR(std::string, "uwsgi: read error receiving response");
        }
    }
    close(sock_fd);
    return OK(std::string, response);
}
