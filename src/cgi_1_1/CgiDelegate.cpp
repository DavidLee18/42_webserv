#include "CgiDelegate.hpp"
#include "../core/Errors.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/wait.h>

CgiDelegate::CgiDelegate(Request const& req, EPoll& ep)
    : _env(), _script_path(), _interpreter(), _req(req), _epoll(ep), _pid(-1),
      _stdin(NULL), _stdout(NULL), _total_written(0), _output(),
      _state(NotRegistered), _start_time(), _timeout_ns(0) {}

Result<CgiDelegate> CgiDelegate::from_req(
    const Request& req, EPoll& ep, const RouteRule_CGI& rule,
    std::map<std::string, std::string> const& cgi_interpreters, char** envp) {
    CgiDelegate del(req, ep);

    TRY(CgiDelegate, CgiInput, del._env, CgiInput::Parser::parse(req))

    std::string pwd(utils::get_env("PWD", envp));
    if (pwd.empty()) return ERR(CgiDelegate, "getting PWD failed");

    del._script_path          = pwd + rule.get_executable();
    const size_t last_dot_pos = del._script_path.rfind('.');
    if (last_dot_pos == std::string::npos)
        return ERR(CgiDelegate, "no extension for cgi executable");

    const std::string ext(del._script_path.substr(last_dot_pos + 1));
    std::cout << utils::debug << "found script\'s extension: " << ext
              << std::endl;

    std::map<std::string, std::string>::const_iterator it =
        cgi_interpreters.find(ext);
    if (it != cgi_interpreters.end()) {
        std::cout << utils::debug << "found interpreter: \"" << it->second
                  << "\"" << std::endl;
        del._interpreter = it->second;
        del._env.add_mvar("SCRIPT_FILENAME", del._script_path);
    }

    if (rule.get_timeout_ms() == 0)
        return ERR(CgiDelegate, "timeout must be positive");
    del._timeout_ns = static_cast<size_t>(rule.get_timeout_ms() * 1e6);

    std::map<std::string, std::string> vars(rule.get_env());
    for (std::map<std::string, std::string>::const_iterator it = vars.begin();
         it != vars.end(); ++it) {
        del._env.add_mvar(it->first, it->second);
    }

    return OK(CgiDelegate, del);
}

CgiDelegate::CgiDelegate(const CgiDelegate& other)
    : _env(other._env), _script_path(other._script_path),
      _interpreter(other._interpreter), _req(other._req), _epoll(other._epoll),
      _pid(other._pid), _stdin(other._stdin), _stdout(other._stdout),
      _total_written(other._total_written), _output(other._output),
      _state(other._state), _start_time(other._start_time),
      _timeout_ns(other._timeout_ns) {
    const_cast<CgiDelegate&>(other)._env.mvars.clear();
    const_cast<CgiDelegate&>(other)._env.req_body.clear();
    const_cast<CgiDelegate&>(other)._script_path.clear();
    const_cast<CgiDelegate&>(other)._pid           = -1;
    const_cast<CgiDelegate&>(other)._stdin         = NULL;
    const_cast<CgiDelegate&>(other)._stdout        = NULL;
    const_cast<CgiDelegate&>(other)._total_written = 0;
    const_cast<CgiDelegate&>(other)._output.clear();
    const_cast<CgiDelegate&>(other)._state = NotRegistered;
}

CgiDelegate&
CgiDelegate::operator=(const CgiDelegate& other) throw(std::logic_error) {
    (void)other;
    throw std::logic_error("CgiDelegate assignment not allowed");
}

// Phase 1: create pipes, fork the CGI process, and register the parent's
// pipe ends with the shared epoll instance. No epoll_wait() is performed
// here - the caller's main loop is the sole owner of epoll_wait() and
// will drive handle_event() for each event delivered.
Result<Void> CgiDelegate::register_(
    std::map<FileDescriptor const*,
             std::pair<FileDescriptor const*, CgiDelegate*> >& cgis,
    FileDescriptor const*                                      client_fd) {
    if (_pid != -1 || _state != NotRegistered)
        return ERR(Void, Errors::cgi_invalid_state);

    Result<std::pair<FileDescriptor, FileDescriptor> > stdin_pipe_res =
        FileDescriptor::pipe();
    Result<std::pair<FileDescriptor, FileDescriptor> > stdout_pipe_res =
        FileDescriptor::pipe();

    if (!stdin_pipe_res.has_value())
        return ERR(Void, "Failed to create stdin pipe");

    if (!stdout_pipe_res.has_value()) {
        {
            FileDescriptor stdin0(stdin_pipe_res.value().first);
            FileDescriptor stdin1(stdin_pipe_res.value().second);
        }
        return ERR(Void, "Failed to create stdout pipe");
    }

    if (!const_cast<FileDescriptor&>(stdin_pipe_res.value().first)
             .close_on_exec()
             .has_value()) {
        {
            FileDescriptor stdin0(stdin_pipe_res.value().first);
            FileDescriptor stdin1(stdin_pipe_res.value().second);
            FileDescriptor stdout0(stdout_pipe_res.value().first);
            FileDescriptor stdout1(stdout_pipe_res.value().second);
        }
        return ERR(Void, "Failed to set stdin pipe to close-on-exec mode");
    }

    if (!const_cast<FileDescriptor&>(stdout_pipe_res.value().second)
             .close_on_exec()
             .has_value()) {
        {
            FileDescriptor stdin0(stdin_pipe_res.value().first);
            FileDescriptor stdin1(stdin_pipe_res.value().second);
            FileDescriptor stdout0(stdout_pipe_res.value().first);
            FileDescriptor stdout1(stdout_pipe_res.value().second);
        }
        return ERR(Void, "Failed to set stdout pipe to close-on-exec mode");
    }

    std::cout << utils::debug << "interpreter: \"" << _interpreter << "\""
              << std::endl;

    pid_t pid = fork();
    if (pid == -1) {
        {
            FileDescriptor stdin0(stdin_pipe_res.value().first);
            FileDescriptor stdin1(stdin_pipe_res.value().second);
            FileDescriptor stdout0(stdout_pipe_res.value().first);
            FileDescriptor stdout1(stdout_pipe_res.value().second);
        }
        return ERR(Void, "Failed to fork process");
    } else if (pid == 0) {
        {
            FileDescriptor stdin1  = stdin_pipe_res.value().second;
            FileDescriptor stdout0 = stdout_pipe_res.value().first;
        }

        FileDescriptor& stdin =
            const_cast<FileDescriptor&>(stdin_pipe_res.value().first);
        FileDescriptor& stdout =
            const_cast<FileDescriptor&>(stdout_pipe_res.value().second);

        Result<Void> res = stdin.dup2stdin();
        if (!res.has_value()) {
            std::cerr << res.error() << std::endl;
            std::exit(1);
        }

        res = stdout.dup2stdout();
        if (!res.has_value()) {
            std::cerr << res.error() << std::endl;
            std::exit(1);
        }

        char** envp = _env.to_envp();
        char** argv;
        if (_interpreter.empty()) {
            argv    = new char*[2];
            argv[0] = const_cast<char*>(_script_path.c_str());
            argv[1] = NULL;
        } else {
            argv    = new char*[3];
            argv[0] = const_cast<char*>(_interpreter.c_str());
            argv[1] = const_cast<char*>(_script_path.c_str());
            argv[2] = NULL;
        }

        size_t      last_slash = _script_path.rfind('/');
        std::string dir_path;
        if (last_slash == std::string::npos) {
            const std::string pwd(utils::get_env("PWD", envp));
            dir_path = pwd.empty() ? "/" : pwd + "/";
        } else
            dir_path = _script_path.substr(0, last_slash + 1);

        if (chdir(dir_path.c_str()) != 0) {
            std::cerr << "Failed to change directory to: " << dir_path
                      << std::endl;
            std::exit(1);
        }

        std::cout << utils::debug << "executing \"" << argv[0] << "\" with \""
                  << argv[1] << "\"" << std::endl;

        execve(argv[0], argv, envp);

        // execve failed
        delete[] argv;
        for (size_t i = 0; envp[i] != NULL; i++) delete[] envp[i];
        delete[] envp;

        if (_interpreter.empty())
            std::cerr << "Failed to execute CGI script: " << _script_path
                      << std::endl;
        else
            std::cerr << "Failed to run CGI interpreter: " << _interpreter
                      << " with script: " << _script_path << std::endl;
        exit(1);
    }

    {
        FileDescriptor _stdin0(stdin_pipe_res.value().first);
        FileDescriptor _stdout1(stdout_pipe_res.value().second);
    }

    _pid                  = pid;
    FileDescriptor stdin  = stdin_pipe_res.value().second;
    FileDescriptor stdout = stdout_pipe_res.value().first;

    Result<Void>   res    = stdin.set_nonblocking();
    if (!res.has_value()) {
        _state = Failed;
        return ERR(Void, "Failed to set stdin pipe to non-blocking mode");
    }

    res = stdout.set_nonblocking();
    if (!res.has_value()) {
        _state = Failed;
        return ERR(Void, "Failed to set stdout pipe to non-blocking mode");
    }

    // Register stdin for EPOLLOUT only when we actually have a body to send.
    // If there is no body, let the stdin_fd destructor close the pipe so the
    // CGI script sees EOF on its stdin.
    if (!_req.get_body().empty()) {
        Event  write_event(NULL, false, true, false, false, true, true);
        Option write_option(false, false, false, false);

        Result<FileDescriptor*> add_res =
            _epoll.add_fd(stdin, write_event, write_option);
        if (!add_res.has_value()) {
            _state = Failed;
            return ERR(Void, "Failed to add stdin to epoll");
        }

        _stdin = add_res.value();
    } else {
        { FileDescriptor stdin_drop(stdin); }
        _stdin = NULL;
    }

    // Register stdout for EPOLLIN (plus err/hup so we notice child exit).
    Event  read_event(NULL, true, false, true, false, true, true);
    Option read_option(false, false, false, false);

    Result<FileDescriptor*> add_out_res =
        _epoll.add_fd(stdout, read_event, read_option);
    if (!add_out_res.has_value()) {
        if (_stdin != NULL) {
            std::cerr << utils::debug << "closing stdin: fd=" << _stdin->_fd
                      << std::endl;
            const int stdin = _stdin->_fd;
            _epoll.del_fd(_stdin);
            const int r = fcntl(stdin, F_GETFD);
            std::cerr << utils::debug << "F_GETFD stdin(" << stdin << "): " << r
                      << " (" << strerror(errno) << ')' << std::endl;
            _stdin = NULL;
        }
        _state = Failed;
        return ERR(Void, "Failed to add stdout to epoll");
    }
    _stdout = add_out_res.value();

    if (clock_gettime(CLOCK_MONOTONIC, &_start_time) != 0) {
        _state = Failed;
        return ERR(Void, "Failed to get start time for CGI process");
    }

    cgis[_stdin]  = std::make_pair(client_fd, this);
    cgis[_stdout] = std::make_pair(client_fd, this);
    _state        = Waiting;
    return OKV;
}

// Phase 2: consume one event delivered by the caller's shared epoll_wait.
// Caller is expected to filter events and only forward those belonging to
// fds this delegate registered. Unknown events are ignored.
Result<Void> CgiDelegate::handle_event(const Event* ev) {
    std::cerr << utils::debug
              << "handle_event called: ev_fd=" << (ev ? ev->fd->_fd : -1)
              << " stdin=" << (_stdin ? _stdin->_fd : -1)
              << " stdout=" << (_stdout ? _stdout->_fd : -1) << std::endl;

    if (ev == NULL) return OKV;
    if (_state == Failed) return ERR(Void, Errors::bad_gateway);
    if (_state != Waiting) return ERR(Void, Errors::invalid_operation);

    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        _state = Failed;
        return ERR(Void, Errors::bad_gateway);
    }
    if (static_cast<size_t>(now.tv_sec * 1000000000 + now.tv_nsec) >=
        _timeout_ns + static_cast<size_t>(_start_time.tv_sec * 1000000000 +
                                          _start_time.tv_nsec)) {
        _state = Failed;
        return ERR(Void, Errors::gateway_timeout);
    }
    if (_stdin != NULL && ev->fd == _stdin) {
        if (ev->err) {
            _state = Failed;
            return ERR(Void, Errors::bad_gateway);
        }
        if (ev->hup || ev->rdhup) {
            if (_total_written < _req.get_body().length()) {
                _state = Failed;
                return ERR(Void, Errors::bad_gateway);
            }
            // All data was already written; close stdin and continue.

            std::cerr << utils::debug << "closing stdin: fd=" << _stdin->_fd
                      << std::endl;
            const int stdin = _stdin->_fd;
            _epoll.del_fd(_stdin);
            const int r = fcntl(stdin, F_GETFD);
            std::cerr << utils::debug << "F_GETFD stdin(" << stdin << "): " << r
                      << " (" << strerror(errno) << ')' << std::endl;
            _stdin = NULL;
            return OKV;
        }
        if (ev->out) {
            if (_total_written < _req.get_body().length()) {
                const Result<ssize_t> written = _stdin->pipe_write(
                    _req.get_body().c_str() + _total_written,
                    _req.get_body().length() - _total_written);

                if (written.has_value() && written.value() > 0)
                    _total_written += static_cast<size_t>(written.value());
                else if (written.has_value() && written.value() == 0) {
                    kill(_pid, SIGKILL);
                    _state = Reaping;
                    if (wait_or_reap()) _state = Failed;
                    return ERR(Void, Errors::bad_gateway);
                } else {
                    _state = Failed;
                    return ERR(Void, Errors::bad_gateway);
                }
            } else {
                // Done writing: drop stdin from epoll, which also closes the
                // pipe, signalling EOF to the CGI script.
                std::cerr << utils::debug << "closing stdin: fd=" << _stdin->_fd
                          << std::endl;
                const int stdin = _stdin->_fd;
                _epoll.del_fd(_stdin);
                const int r = fcntl(stdin, F_GETFD);
                std::cerr << utils::debug << "F_GETFD stdin(" << stdin
                          << "): " << r << " (" << strerror(errno) << ')'
                          << std::endl;
                _stdin = NULL;
                return OKV;
            }
        }
    }
    if (_stdout != NULL && ev->fd == _stdout) {
        // is_stdout
        std::cerr << utils::debug << "stdout event: in=" << ev->in
                  << " hup=" << ev->hup << std::endl;
        if (ev->in || ev->hup || ev->rdhup) {
            char                  buffer[4096];
            const Result<ssize_t> bytes_read =
                _stdout->pipe_read(buffer, sizeof(buffer));

            if (bytes_read.has_value() && bytes_read.value() > 0) {
                _output.append(buffer, static_cast<size_t>(bytes_read.value()));
                return OKV;
            }

            if (!bytes_read.has_value() || bytes_read.value() < 0) {
                kill(_pid, SIGKILL);
                _state = Reaping;
                if (wait_or_reap()) _state = Failed;
                return ERR(Void, Errors::bad_gateway);
            }

            // bytes_read == 0: EOF, drain any remaining IO bookkeeping.
            _epoll.del_fd(_stdout);
            _stdout = NULL;
            if (_stdin != NULL) {
                std::cerr << utils::debug << "closing stdin: fd=" << _stdin->_fd
                          << std::endl;
                const int stdin = _stdin->_fd;
                _epoll.del_fd(_stdin);
                const int r = fcntl(stdin, F_GETFD);
                std::cerr << "F_GETFD stdin(" << stdin << "): " << r << " ("
                          << strerror(errno) << ')' << std::endl;
                _stdin = NULL;
            }
            _state       = Done;

            int status   = 0;
            int wait_res = waitpid(_pid, &status, WNOHANG);

            if (wait_res == 0) {
                kill(_pid, SIGKILL);
                _state = Reaping;
                if (wait_or_reap()) _state = Failed;
                return ERR(Void, Errors::bad_gateway);
            }

            if (wait_res < 0) {
                _state = Failed;
                return ERR(Void, Errors::bad_gateway);
            }

            (void)wait_or_reap();
            return OKV;
        }

        if (ev->err) {
            kill(_pid, SIGKILL);
            _state = Reaping;
            if (wait_or_reap()) _state = Failed;
            return ERR(Void, Errors::bad_gateway);
        }
        return OKV;
    } else
        return OKV; // not for us
}

Result<std::string> CgiDelegate::poll() const {
    switch (_state) {
    case Done: return OK(std::string, _output);
    case Failed: return ERR(std::string, "CGI failed");
    default: return ERR(std::string, Errors::try_again);
    }
}

bool CgiDelegate::check_timeout() {
    if (remaining_ns() == 0) {
        _state = Failed;
        return true;
    }
    return false;
}

size_t CgiDelegate::remaining_ns() const {
    timespec now = {};

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) { return 0; }
    const size_t now_ns =
        static_cast<size_t>(now.tv_sec * 1000000000 + now.tv_nsec);

    const size_t start_ns = static_cast<size_t>(
        _start_time.tv_sec * 1000000000 + _start_time.tv_nsec);

    if (now_ns >= start_ns + _timeout_ns) {
        return 0;
    } else {
        return start_ns + _timeout_ns - now_ns;
    }
}

CgiDelegate::~CgiDelegate() {
    if (_stdin != NULL) {
        _epoll.del_fd(_stdin);
        _stdin = NULL;
    }

    if (_stdout != NULL) {
        _epoll.del_fd(_stdout);
        _stdout = NULL;
    }

    if (_pid > 0) {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _pid = -1;
    }
}

bool CgiDelegate::wait_or_reap() {
    if (_state == Done || _state == Failed)
        return true;
    else if (_state != Reaping)
        return false;
    int       status    = 0;
    const int wait_stat = waitpid(_pid, &status, WNOHANG);
    if (wait_stat == 0) return false;
    if (wait_stat < 0) {
        _state = Failed;
        return false;
    } else if (!WIFEXITED(status))
        return false;
    else {
        const int code = WEXITSTATUS(status);
        _state         = code == 0 ? Done : Failed;
        _pid           = -1;
        return true;
    }
}
