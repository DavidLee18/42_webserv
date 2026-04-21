#ifndef UWSGI_H
#define UWSGI_H

#include "http_1_1.h"
#include "result.h"
#include "server/Client.hpp"
#include <list>
#include <map>
#include <string>
#include <vector>

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
  Request const &_req;

private:
  UwsgiInput();
  UwsgiInput(std::vector<UwsgiMetaVar>, Request const &);
  UwsgiInput(Request const &);

public:
  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<UwsgiInput> parse(Request const &);
  };

  friend class Parser;
  friend class UwsgiDelegate;

  UwsgiInput(const UwsgiInput &other) : mvars(other.mvars), _req(other._req) {}
  UwsgiInput &operator=(const UwsgiInput &other) {
    if (this != &other) {
      mvars = other.mvars;
      _req = other._req;
    }
    return *this;
  }
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
  std::map<std::string, std::string> to_map() const;
};

class UwsgiDelegate {
  UwsgiInput _env;
  unsigned short _port;
  Request const &_req;

  EPoll &_epoll; // borrowed, not owned
  FileDescriptor *_sock;
  std::vector<unsigned char> _send_buf;
  size_t _total_sent;
  std::string _output;
  std::string _error;
  bool _complete;

  UwsgiDelegate(EPoll &, Request const &);

public:
  Result<UwsgiDelegate> from_req(EPoll &, Request const &, unsigned short);

  // Phase 1: create the socket, issue a non-blocking connect to the uwsgi
  // server, and register the socket with the shared epoll. Does NOT call
  // epoll->wait().
  Result<Void> register_();

  // Phase 2: process a single epoll event delivered by the main loop's
  // shared epoll_wait. Drives the connect -> send -> receive state
  // machine; performs only non-blocking IO.
  Result<Void> handle_event(const Event *ev);

  // Retrieve the parsed HTTP response.
  Result<std::string> poll() const;

  ~UwsgiDelegate();
};

#endif
