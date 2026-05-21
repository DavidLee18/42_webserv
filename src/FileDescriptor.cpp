#include "FileDescriptor.hpp"
#include "Errors.hpp"
#include "utils.hpp"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <netdb.h>

Result<FileDescriptor> FileDescriptor::socket_new() {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        switch (errno) {
        case EACCES: return ERR(FileDescriptor, Errors::access_denied);
        case EAFNOSUPPORT:
        case EPROTONOSUPPORT: return ERR(FileDescriptor, Errors::not_supported);
        case EINVAL: return ERR(FileDescriptor, Errors::invalid_operation);
        case EMFILE:
        case ENFILE: return ERR(FileDescriptor, Errors::fd_too_many);
        case ENOBUFS:
        case ENOMEM: return ERR(FileDescriptor, Errors::out_of_mem);
        }
    }
    FileDescriptor fd;
    fd._fd = sock;
    return OK(FileDescriptor, fd);
}

Result<std::pair<FileDescriptor, addrinfo*> >
FileDescriptor::socket_client_new(std::string const& host,
                                  std::string const& port) {
    addrinfo hints = {}, *res = NULL;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int ai_res;
    if ((ai_res = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) != 0)
        return ERR_PAIR(FileDescriptor, addrinfo*, gai_strerror(ai_res));
    const int _sock =
        socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (_sock < 0) {
        freeaddrinfo(res);
        return ERR_PAIR(FileDescriptor, addrinfo*, strerror(errno));
    }
    FileDescriptor sock;
    sock._fd = _sock;
    return OK_PAIR(FileDescriptor, addrinfo*, sock, res);
}

Result<FileDescriptor> FileDescriptor::from_raw(const int raw_fd) {
    if (raw_fd < 0) return ERR(FileDescriptor, Errors::invalid_fd);
    FileDescriptor fd;
    fd._fd = raw_fd;
    return OK(FileDescriptor, fd);
}

Result<FileDescriptor> FileDescriptor::open_file(std::string const& path,
                                                 char**             envp) {
    // Extract just the filename (basename) to prevent directory traversal.
    const std::string::size_type slash_pos = path.rfind('/');
    const std::string            filename =
        (slash_pos == std::string::npos) ? path : path.substr(slash_pos + 1);

    // Only allow files with the ".wbsrv" extension.
    static const std::string ext = ".wbsrv";
    if (filename.length() < ext.length() ||
        filename.compare(filename.length() - ext.length(), ext.length(), ext) !=
            0) {
        return ERR(FileDescriptor,
                   "server config file extension must be \"wbsrv\"");
    }

    // Build the safe path: current working directory + "/" + basename.
    std::string cwd(utils::get_env("PWD", envp));
    if (cwd.empty())
        return ERR(FileDescriptor, "cannot retrieve current working directory");
    const std::string safe_path = cwd + "/" + filename;

    // Use stat to inspect the path without following symlinks.
    struct stat       st        = {};
    if (stat(safe_path.c_str(), &st) != 0) {
        // Path does not exist or is otherwise invalid.
        return ERR(FileDescriptor,
                   "config file does not exist or the file path is invalid");
    }
    if (!S_ISREG(st.st_mode)) {
        // Ensure the target is a regular file.
        return ERR(FileDescriptor, "the config file is not a regular file");
    }

    // O_NOFOLLOW provides defense-in-depth against TOCTOU races with symlinks.
    const int _fd = open(safe_path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (_fd < 0) return ERR(FileDescriptor, "failed to open the config file");

    FileDescriptor fd;
    fd._fd   = _fd;
    FILE* fp = fdopen(_fd, "r");
    if (fp == NULL) {
        switch (errno) {
        case EMFILE: return ERR(FileDescriptor, Errors::stream_too_many);
        case EBADF: return ERR(FileDescriptor, Errors::invalid_fd);
        case ENOMEM: return ERR(FileDescriptor, Errors::out_of_mem);
        default:
            std::stringstream ss("an unknown error occured; errno: ");
            ss << errno;
            return ERR(FileDescriptor, ss.str());
        }
    }
    fd.fp = fp;
    return OK(FileDescriptor, fd);
}

Result<std::pair<FileDescriptor, FileDescriptor> > FileDescriptor::pipe() {
    int pipes[2];
    if (::pipe(pipes) != 0) {
        switch (errno) {
        case EFAULT:
        case EINVAL:
            return ERR_PAIR(FileDescriptor, FileDescriptor,
                            Errors::invalid_format);
        case EMFILE:
        case ENFILE:
            return ERR_PAIR(FileDescriptor, FileDescriptor,
                            Errors::fd_too_many);
        default:
            return ERR_PAIR(FileDescriptor, FileDescriptor,
                            Errors::invalid_operation);
        }
    }
    FileDescriptor pread;
    pread._fd = pipes[0];
    FileDescriptor pwrite;
    pwrite._fd = pipes[1];
    return OK_PAIR(FileDescriptor, FileDescriptor, pread, pwrite);
}

// Move-like copy constructor: transfers ownership from other
FileDescriptor::FileDescriptor(const FileDescriptor& other)
    : _fd(other._fd), fp(other.fp) {
    // Invalidate source to transfer ownership (cast away const for move
    // semantics)
    (const_cast<FileDescriptor&>(other))._fd = -1;
    (const_cast<FileDescriptor&>(other)).fp  = NULL;
}

// Move-like assignment operator: transfers ownership from other
FileDescriptor& FileDescriptor::operator=(const FileDescriptor& other) {
    if (this != &other) {
        // Close current fd if valid
        if (fp != NULL)
            std::fclose(fp);
        else if (_fd >= 0)
            ::close(_fd);

        // Transfer ownership
        _fd                                      = other._fd;
        fp                                       = other.fp;
        // Invalidate source (cast away const for move semantics)
        (const_cast<FileDescriptor&>(other))._fd = -1;
        (const_cast<FileDescriptor&>(other)).fp  = NULL;
    }
    return *this;
}

FileDescriptor::~FileDescriptor() {
    if (fp != NULL)
        std::fclose(fp);
    else if (_fd >= 0)
        ::close(_fd);
}

Result<Void> FileDescriptor::socket_bind(const in_addr        addr,
                                         const unsigned short port) const {
    sockaddr_in _addr = {};
    _addr.sin_family  = AF_INET;
    _addr.sin_addr    = addr;
    _addr.sin_port    = htons(port);
    if (bind(_fd, reinterpret_cast<const sockaddr*>(&_addr), sizeof(_addr)) <
        0) {
        switch (errno) {
        case EACCES: return ERR(Void, Errors::access_denied);
        case EADDRINUSE:
        case EADDRNOTAVAIL: return ERR(Void, Errors::addr_not_available);
        case EBADF:
        case ENOTSOCK: return ERR(Void, Errors::invalid_fd);
        case EINVAL: return ERR(Void, Errors::invalid_operation);
        case EFAULT: return ERR(Void, Errors::address_fault);
        case ELOOP: return ERR(Void, Errors::addr_loop);
        case ENAMETOOLONG: return ERR(Void, Errors::name_too_long);
        case ENOENT:
        case ENOTDIR: return ERR(Void, Errors::not_found);
        case ENOMEM: return ERR(Void, Errors::out_of_mem);
        case EROFS: return ERR(Void, Errors::readonly_filesys);
        }
    }
    return OKV;
}

Result<Void> FileDescriptor::socket_listen(const unsigned short backlog) {
    if (listen(_fd, backlog) < 0) {
        switch (errno) {
        case EADDRINUSE: return ERR(Void, Errors::addr_not_available);
        case EBADF:
        case ENOTSOCK: return ERR(Void, Errors::invalid_fd);
        case EOPNOTSUPP: return ERR(Void, Errors::not_supported);
        }
    }
    return OKV;
}

Result<FileDescriptor> FileDescriptor::socket_accept(sockaddr*  addr,
                                                     socklen_t* len) const {
    const int fd = accept(_fd, addr, len);
    if (fd >= 0) {
        FileDescriptor fd_;
        fd_._fd = fd;
        return OK(FileDescriptor, fd_);
    }
    switch (errno) {
    case EWOULDBLOCK: return ERR(FileDescriptor, Errors::try_again);
    case EBADF:
    case ENOTSOCK: return ERR(FileDescriptor, Errors::invalid_fd);
    case ECONNABORTED: return ERR(FileDescriptor, Errors::conn_aborted);
    case EFAULT: return ERR(FileDescriptor, Errors::address_fault);
    case EINTR: return ERR(FileDescriptor, Errors::interrupted);
    case EINVAL:
        return ERR(
            FileDescriptor,
            "Socket is not listening for connections, or addrlen is invalid.");
    case EMFILE:
    case ENFILE: return ERR(FileDescriptor, Errors::fd_too_many);
    case ENOBUFS:
    case ENOMEM: return ERR(FileDescriptor, Errors::out_of_mem);
    case EOPNOTSUPP:
        return ERR(FileDescriptor,
                   "The referenced socket is not of type SOCK_STREAM.");
    case EPERM: return ERR(FileDescriptor, Errors::access_denied);
    default:
        return ERR(FileDescriptor, "an unknown error occured during accept().");
    }
}

Result<Void> FileDescriptor::socket_connect(const addrinfo* ad_info) const {
    if (connect(_fd, ad_info->ai_addr, ad_info->ai_addrlen) == 0) return OKV;
    return ERR(Void, strerror(errno));
}

Result<ssize_t> FileDescriptor::sock_recv(void* buf, const size_t size) const {
    const ssize_t res = recv(_fd, buf, size, 0);
    if (res < 0) return ERR(ssize_t, "`recv` failed: ");
    return OK(ssize_t, res);
}

Result<ssize_t> FileDescriptor::pipe_read(void* buf, const size_t size) const {
    const ssize_t res = read(_fd, buf, size);
    if (res < 0) return ERR(ssize_t, "`read` failed");
    return OK(ssize_t, res);
}

Result<std::string> FileDescriptor::try_read_to_end() const {
    std::stringstream ss;
    char              buf[BUFFER_SIZE];

    Result<ssize_t>   bytes = this->sock_recv(buf, BUFFER_SIZE);
    while (bytes.error().empty() && bytes.value() > 0) {
        ssize_t bs;
        TRY(std::string, ssize_t, bs, bytes)
        char* s = new char[static_cast<size_t>(bs + 1)];
        s       = std::strncpy(s, buf, static_cast<size_t>(bs + 1));
        if (!(ss << s)) return ERR(std::string, "string concat failed");
        delete[] s;
        bytes = this->sock_recv(buf, BUFFER_SIZE);
    }
    if (!bytes.error().empty()) return ERR(std::string, bytes.error());
    if (bytes.value() == 0) return OK(std::string, ss.str());
    return OK(std::string, ss.str());
}

Result<Void> FileDescriptor::set_nonblocking() {
    const int flags = fcntl(_fd, F_GETFL, 0);
    if (flags < 0) { return ERR(Void, "Failed to get file descriptor flags"); }
    if (fcntl(_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return ERR(Void, "Failed to set non-blocking mode");
    }
    return OKV;
}

Result<Void> FileDescriptor::set_socket_option(const int       level,
                                               const int       optname,
                                               const void*     optval,
                                               const socklen_t optlen) {
    if (setsockopt(_fd, level, optname, optval, optlen) < 0) {
        return ERR(Void, "Failed to set socket option");
    }
    return OKV;
}

Result<ssize_t> FileDescriptor::sock_send(const void*  buf,
                                          const size_t size) const {
    const ssize_t res = send(_fd, buf, size, 0);
    if (res < 0) return ERR(ssize_t, "send failed");
    return OK(ssize_t, res);
}

Result<ssize_t> FileDescriptor::pipe_write(void const*  buf,
                                           const size_t size) const {
    const ssize_t res = write(_fd, buf, size);
    if (res < 0) return ERR(ssize_t, "write to pipe failed");
    return OK(ssize_t, res);
}

Result<std::string> FileDescriptor::read_file_line() const {
    if (fp == NULL) return ERR(std::string, "FILE not initialized");
    std::string res;
    char*       buf = new char[4097];
    std::memset(reinterpret_cast<void*>(buf), 0, 4097);
    while (std::fgets(buf, 4096, fp)) {
        res += buf;
        if (std::strlen(buf) < 4096)
            return (delete[] buf, OK(std::string, res));
        delete[] buf;
        buf = new char[4097];
    }
    if (!std::feof(fp))
        return (
            delete[] buf,
            ERR(std::string,
                "file read failed")); // TODO: specify error kind and message
    res += buf;
    delete[] buf;
    return OK(std::string, res);
}

Result<Void> FileDescriptor::dup2stdin() {
    if (dup2(_fd, STDIN_FILENO) < 0) return ERR(Void, "dup2 to stdin failed");
    return OKV;
}

Result<Void> FileDescriptor::dup2stdout() {
    if (dup2(_fd, STDOUT_FILENO) < 0) return ERR(Void, "dup2 to stdout failed");
    return OKV;
}

bool operator==(const int& lhs, const FileDescriptor& rhs) {
    return lhs == rhs._fd;
}

bool operator!=(const int& lhs, const FileDescriptor& rhs) {
    return !(lhs == rhs);
}

Result<Void> FileDescriptor::close_on_exec() {
    if (fcntl(_fd, F_SETFD, FD_CLOEXEC) < 0)
        return ERR(Void, "Failed to set close-on-exec flag");
    return OKV;
}
