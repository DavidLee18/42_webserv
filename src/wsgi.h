#ifndef WSGI_H
#define WSGI_H

#include "http_1_1.h"
#include "result.h"
#include <list>
#include <map>
#include <string>
#include <vector>

// Forward declarations
class WsgiInput;
class EPoll;

class WsgiMetaVar {
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
    static Result<std::pair<WsgiMetaVar, size_t> > parse(std::string const &,
                                                        std::string const &);
  };

  friend class WsgiInput;

public:
  WsgiMetaVar(const WsgiMetaVar &other);
  WsgiMetaVar &operator=(const WsgiMetaVar &other);
  ~WsgiMetaVar();

  Name const &get_name() const { return name; }
  std::string const &get_value() const { return value; }

private:
  Name name;
  std::string value;

  WsgiMetaVar(Name n, std::string v) : name(n), value(v) {}
  static WsgiMetaVar create(Name n, std::string v);
};

class WsgiInput {
  std::vector<WsgiMetaVar> mvars;
  Http::Body req_body;

private:
  WsgiInput();
  WsgiInput(std::vector<WsgiMetaVar>, Http::Body);
  WsgiInput(Http::Request const &);

public:
  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<WsgiInput> parse(Http::Request const &);
  };

  friend class Parser;
  friend class WsgiDelegate;

  WsgiInput(const WsgiInput &other)
      : mvars(other.mvars), req_body(other.req_body) {}
  WsgiInput &operator=(const WsgiInput &other) {
    if (this != &other) {
      mvars = other.mvars;
      req_body = other.req_body;
    }
    return *this;
  }
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class WsgiDelegate {
  WsgiInput env;
  std::string script_path;
  Http::Request request;

public:
  WsgiDelegate(const Http::Request &req, const std::string &script);
  Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
  ~WsgiDelegate();
};

#endif
