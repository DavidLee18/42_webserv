#ifndef UWSGI_H
#define UWSGI_H

#include "http_1_1.h"
#include "result.h"
#include <list>
#include <map>
#include <string>
#include <vector>

// Forward declarations
class UwsgiInput;
class EPoll;
class Event;
class FileDescriptor;

class UwsgiMetaVar {
public:
  enum Name {
    REQUEST_METHOD,
    SCRIPT_NAME,
    PATH_INFO,
    QUERY_STRING,
    CONTENT_TYPE,
    CONTENT_LENGTH,
    SERVER_NAME,
    SERVER_PORT,
    SERVER_PROTOCOL,
    HTTP_, // HTTP headers
    REMOTE_ADDR,
    WSGI_VERSION,
    WSGI_URL_SCHEME,
    WSGI_INPUT,
    WSGI_ERRORS,
    WSGI_MULTITHREAD,
    WSGI_MULTIPROCESS,
    WSGI_RUN_ONCE,
  };

  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<std::pair<UwsgiMetaVar, size_t> > parse(std::string const &,
                                                          std::string const &);
  };

  friend class UwsgiInput;

public:
  UwsgiMetaVar(const UwsgiMetaVar &other);
  UwsgiMetaVar &operator=(const UwsgiMetaVar &other);
  ~UwsgiMetaVar();

  Name const &get_name() const { return name; }
  std::string const &get_value() const { return value; }

private:
  Name name;
  std::string value;

  UwsgiMetaVar(Name n, std::string v) : name(n), value(v) {}
  static UwsgiMetaVar create(Name n, std::string v);
};

class UwsgiInput {
  std::vector<UwsgiMetaVar> mvars;
  Http::Body req_body;

private:
  UwsgiInput();
  UwsgiInput(std::vector<UwsgiMetaVar>, Http::Body);
  UwsgiInput(Http::Request const &);

public:
  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<UwsgiInput> parse(Http::Request const &);
  };

  friend class Parser;
  friend class UwsgiDelegate;

  UwsgiInput(const UwsgiInput &other)
      : mvars(other.mvars), req_body(other.req_body) {}
  UwsgiInput &operator=(const UwsgiInput &other) {
    if (this != &other) {
      mvars = other.mvars;
      req_body = other.req_body;
    }
    return *this;
  }
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
  std::map<std::string, std::string> to_map() const;
};

class UwsgiDelegate {
public:
  enum State {
    NOT_STARTED,
    CONNECTING,
    SENDING,
    RECEIVING,
    COMPLETE,
    FAILED
  };

private:
  UwsgiInput env;
  int _uwsgi_port;
  Http::Request request;

  State _state;
  EPoll *_epoll;             // borrowed, not owned
  int _raw_sock;             // raw socket fd; -1 when not registered
  FileDescriptor *_sock_epoll; // non-null while registered in epoll
  std::vector<unsigned char> _send_buf;
  size_t _total_sent;
  std::string _output;
  Http::Response *_response; // built lazily on successful completion
  std::string _error;

  void _fail(const std::string &msg);
  void _cleanup_epoll();
  Result<Void> _switch_to_sending();
  Result<Void> _switch_to_receiving();
  Result<Void> _parse_response();

public:
  UwsgiDelegate(const Http::Request &req, int uwsgi_port);

  // Phase 1: create the socket, issue a non-blocking connect to the uwsgi
  // server, and register the socket with the shared epoll. Does NOT call
  // epoll->wait().
  Result<Void> start(EPoll *epoll);

  // Phase 2: process a single epoll event delivered by the main loop's
  // shared epoll_wait. Drives the connect -> send -> receive state
  // machine; performs only non-blocking IO.
  Result<Void> handle_event(const Event *ev);

  bool is_done() const { return _state == COMPLETE || _state == FAILED; }
  State state() const { return _state; }

  // Retrieve the parsed HTTP response. Valid once is_done() is true.
  Result<Http::Response> result() const;

  // DEPRECATED convenience wrapper. Kept so existing synchronous callers
  // compile while they migrate to start()/handle_event(). New code MUST
  // use start()/handle_event() and let the main loop own epoll_wait().
  Result<Http::Response> execute(int timeout_ms, EPoll *epoll);

  ~UwsgiDelegate();
};

#endif
