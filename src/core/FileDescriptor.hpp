#ifndef FILE_DESCRIPTOR_HPP
#define FILE_DESCRIPTOR_HPP

#include "Result.hpp"
#include <sys/socket.h>

#define BUFFER_SIZE 42

class Event;

class FileDescriptor {
    int   _fd;
    FILE* fp;

    FileDescriptor() : _fd(-1), fp(NULL) {}

  public:
    static Result<FileDescriptor> from_raw(int);

    static Result<FileDescriptor> socket_new();

    static Result<std::pair<FileDescriptor, struct addrinfo*> >
    socket_client_new(std::string const&, std::string const&);

    static Result<FileDescriptor> open_file(std::string const&, char** envp);

    static Result<std::pair<FileDescriptor, FileDescriptor> > pipe();

    // Move-like copy constructor: transfers ownership from other
    FileDescriptor(const FileDescriptor&);

    // Move-like assignment operator: transfers ownership from other
    FileDescriptor& operator=(const FileDescriptor&);

    ~FileDescriptor();

    Result<Void>           socket_bind(struct in_addr, unsigned short) const;

    Result<Void>           socket_listen(unsigned short);

    Result<FileDescriptor> socket_accept(sockaddr*, socklen_t*) const;

    Result<Void>           socket_connect(const addrinfo*) const;

    Result<ssize_t>        sock_recv(void*, size_t) const;

    Result<ssize_t>        pipe_read(void*, size_t) const;

    Result<std::string>    try_read_to_end() const;


  // Sets the file descriptor to non-blocking mode.
  Result<Void> set_nonblocking();

    Result<Void>           set_socket_option(int, int, const void*, socklen_t);

    Result<ssize_t>        sock_send(const void*, size_t) const;

    Result<ssize_t>        pipe_write(const void*, size_t) const;

    Result<std::string>    read_file_line() const;

    Result<Void>           dup2stdin();

    Result<Void>           dup2stdout();
    Result<Void>           close_on_exec();

    bool operator==(const int& other) const { return _fd == other; }
    bool operator==(const FileDescriptor& other) const {
        return _fd == other._fd;
    }
    bool operator!=(const int& other) const { return !(*this == other); }
    bool operator!=(const FileDescriptor& other) const {
        return !(*this == other);
    }
    friend bool operator==(const int&, const FileDescriptor&);
    friend bool operator!=(const int&, const FileDescriptor&);

    friend class EPoll;
    friend class CgiDelegate;
    friend class Server;
};

#endif // FILE_DESCRIPTOR_H
