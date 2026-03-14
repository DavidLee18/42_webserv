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
    static Result<std::pair<UwsgiMetaVar, size_t>> parse(std::string const &,
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
  UwsgiInput env;
  std::string _uwsgi_host;
  int _uwsgi_port;
  Http::Request request;

public:
  UwsgiDelegate(const Http::Request &req, const std::string &uwsgi_host,
                int uwsgi_port);
  Result<Http::Response> execute(int timeout_ms, EPoll *epoll);
  ~UwsgiDelegate();
};

#endif
