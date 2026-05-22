#ifndef CGIDELEGATE_HPP
#define CGIDELEGATE_HPP

#include "../core/EPoll.hpp"
#include "../core/FileDescriptor.hpp"
#include "../config/RouteRule_CGI.hpp"
#include "CgiInput.hpp"
#include <map>
#include <string>

class CgiDelegate {
  public:
    enum State {
        NotRegistered,
        Waiting,
        Failed,
        Done,
        Reaping,
    };

    static Result<CgiDelegate>
    from_req(Request const&, EPoll&, RouteRule_CGI const&,
             std::map<std::string, std::string> const&, char** envp);

    CgiDelegate(const CgiDelegate&);
    CgiDelegate& operator=(const CgiDelegate&) throw(std::logic_error);

    // Phase 1: create pipes, fork, register the pipe fds with epoll.
    // After this returns OK, the main event loop will deliver events on the
    // registered fds; the caller must route them to handle_event().
    Result<Void>
                        register_(std::map<FileDescriptor const*,
                       std::pair<FileDescriptor const*, CgiDelegate*> >& cgis,
                                  FileDescriptor const* client_fd);

    // Phase 2: process a single epoll event for this CGI. Performs
    // non-blocking IO only; never calls epoll->wait(). Returns an error if
    // the CGI fails, in which case is_done() also becomes true.
    Result<Void>        handle_event(const Event*);

    Result<std::string> poll() const;

    bool                check_timeout();

    size_t              remaining_ns() const;

    bool                wait_or_reap();

    ~CgiDelegate();

  private:
    CgiInput        _env;
    std::string     _script_path;
    std::string     _interpreter;
    const Request   _req;
    EPoll&          _epoll;
    pid_t           _pid;
    FileDescriptor* _stdin;
    FileDescriptor* _stdout;
    size_t          _total_written;
    std::string     _output;
    State           _state;
    timespec        _start_time;
    size_t          _timeout_ns;

    CgiDelegate(Request const&, EPoll&);
};

#endif
