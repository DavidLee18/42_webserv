#ifndef CGI_1_1_H
#define CGI_1_1_H

#include "WebserverConfig.hpp"
#include "http_1_1.h"
#include "result.h"
#include <list>
#include <map>
#include <string>

// Forward declarations
class CgiInput;
class EPoll;

class CgiAuthType {
public:
  enum Type {
    Basic,
    Digest,
    CgiAuthOther,
  };
  explicit CgiAuthType(Type type) : _type(type), _other(NULL) {}
  explicit CgiAuthType(Type type, std::string other)
      : _type(type), _other(new std::string(other)) {}
  CgiAuthType(const CgiAuthType &other) : _type(other._type) {
    if (other._other != NULL) {
      _other = new std::string(*other._other);
    } else {
      _other = NULL;
    }
  }
  CgiAuthType& operator=(const CgiAuthType &other) {
    if (this != &other) {
      delete _other;
      _type = other._type;
      if (other._other != NULL) {
        _other = new std::string(*other._other);
      } else {
        _other = NULL;
      }
    }
    return *this;
  }
  ~CgiAuthType() {
    delete _other;
  }
  Type const &type() { return _type; }
  std::string const *other() { return _other; }

private:
  Type _type;
  std::string *_other;
};

class ContentType {
public:
  enum Type {
    application,
    audio,
    example,
    font,
    haptics,
    image,
    message,
    model,
    multipart,
    text,
    video
  };
  ContentType(Type ty, std::string subty)
      : type(ty), subtype(subty), params() {}
  ContentType(ContentType const &other)
      : type(other.type), subtype(other.subtype), params(other.params) {}
  ContentType& operator=(const ContentType &other) {
    if (this != &other) {
      type = other.type;
      subtype = other.subtype;
      params = other.params;
    }
    return *this;
  }
  Result<Void> add_param(std::string, std::string);

public:
  Type type;
  std::string subtype;
  std::map<std::string, std::string> params;
};

enum GatewayInterface { Cgi_1_1 };

class ServerName {
public:
  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<ServerName, size_t> > parse_host(std::string);
    static Result<std::pair<ServerName, size_t> > parse_ipv4(std::string);

  public:
    static Result<std::pair<ServerName, size_t> > parse(std::string);
  };

  enum Type {
    Host,
    Ipv4,
  };

  union Val {
    std::list<std::string> *host_name;
    unsigned char ipv4[4];
  };

public:
  ServerName(const ServerName &other) : type(other.type) {
    if (type == Host) {
      val.host_name = new std::list<std::string>(*other.val.host_name);
    } else {
      val.ipv4[0] = other.val.ipv4[0];
      val.ipv4[1] = other.val.ipv4[1];
      val.ipv4[2] = other.val.ipv4[2];
      val.ipv4[3] = other.val.ipv4[3];
    }
  }
  
  ServerName& operator=(const ServerName &other) {
    if (this != &other) {
      // Clean up existing value
      if (type == Host) {
        delete val.host_name;
      }
      
      // Copy new value
      type = other.type;
      if (type == Host) {
        val.host_name = new std::list<std::string>(*other.val.host_name);
      } else {
        val.ipv4[0] = other.val.ipv4[0];
        val.ipv4[1] = other.val.ipv4[1];
        val.ipv4[2] = other.val.ipv4[2];
        val.ipv4[3] = other.val.ipv4[3];
      }
    }
    return *this;
  }
  
  ~ServerName() {
    if (type == Host) {
      delete val.host_name;
    }
  }

private:
  Type type;
  Val val;

  explicit ServerName(Type ty, Val v) : type(ty), val(v) {}

  static ServerName host(std::list<std::string>);
  static ServerName ipv4(unsigned char, unsigned char, unsigned char,
                          unsigned char);
};

enum ServerProtocol { Http_1_1 };

enum ServerSoftware { Webserv };

class EtcMetaVar {

public:
  enum Type { Http, Custom };
  EtcMetaVar(Type ty, std::string n, std::string v)
      : type(ty), name(n), value(v) {}
  EtcMetaVar(EtcMetaVar const &other)
      : type(other.type), name(other.name), value(other.value) {}
  EtcMetaVar& operator=(const EtcMetaVar &other) {
    if (this != &other) {
      type = other.type;
      name = other.name;
      value = other.value;
    }
    return *this;
  }
  
  Type const &get_type() const { return type; }
  std::string const &get_name() const { return name; }
  std::string const &get_value() const { return value; }

private:
  Type type;
  std::string name;
  std::string value;
};

class CgiMetaVar {
public:
  enum Name {
    AUTH_TYPE,
    CONTENT_LENGTH,
    CONTENT_TYPE,
    GATEWAY_INTERFACE,
    PATH_INFO,
    PATH_TRANSLATED,
    QUERY_STRING,
    REMOTE_ADDR,
    REMOTE_HOST,
    REMOTE_IDENT,
    REMOTE_USER,
    REQUEST_METHOD,
    SCRIPT_NAME,
    SERVER_NAME,
    SERVER_PORT,
    SERVER_PROTOCOL,
    SERVER_SOFTWARE,
    X_, // Custom var
  };

  union Val {
    CgiAuthType *auth_type;
    unsigned int content_length;
    ContentType *content_type;
    GatewayInterface gateway_interface;
    std::list<std::string> *path_info;
    std::string *path_translated;
    std::map<std::string, std::string> *query_string;
    unsigned char remote_addr[4];
    std::list<std::string> *remote_host;
    std::string *remote_ident;
    std::string *remote_user;
    Http::Method request_method;
    std::list<std::string> *script_name;
    ServerName *server_name;
    unsigned short server_port;
    ServerProtocol server_protocol;
    ServerSoftware server_software;
    EtcMetaVar *etc_val;
  };

  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<CgiMetaVar, size_t> > parse_auth_type(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_length(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_content_type(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_gateway_interface(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_path_info(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_path_translated(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_query_string(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_addr(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_host(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_ident(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_remote_user(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_request_method(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_script_name(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_name(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_port(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_protocol(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_server_software(std::string);
    static Result<std::pair<CgiMetaVar, size_t> >
        parse_custom_var(std::string, std::string);

  public:
    static Result<std::pair<CgiMetaVar, size_t> >
        parse(std::string const &, std::string const &);
  };
  
  friend class CgiInput;
  
public:
  CgiMetaVar(const CgiMetaVar &other) : name(other.name) {
    switch (name) {
    case AUTH_TYPE:
      val.auth_type = new CgiAuthType(*other.val.auth_type);
      break;
    case CONTENT_LENGTH:
      val.content_length = other.val.content_length;
      break;
    case CONTENT_TYPE:
      val.content_type = new ContentType(*other.val.content_type);
      break;
    case GATEWAY_INTERFACE:
      val.gateway_interface = other.val.gateway_interface;
      break;
    case PATH_INFO:
      val.path_info = new std::list<std::string>(*other.val.path_info);
      break;
    case PATH_TRANSLATED:
      val.path_translated = new std::string(*other.val.path_translated);
      break;
    case QUERY_STRING:
      val.query_string = new std::map<std::string, std::string>(*other.val.query_string);
      break;
    case REMOTE_ADDR:
      val.remote_addr[0] = other.val.remote_addr[0];
      val.remote_addr[1] = other.val.remote_addr[1];
      val.remote_addr[2] = other.val.remote_addr[2];
      val.remote_addr[3] = other.val.remote_addr[3];
      break;
    case REMOTE_HOST:
      val.remote_host = new std::list<std::string>(*other.val.remote_host);
      break;
    case REMOTE_IDENT:
      val.remote_ident = new std::string(*other.val.remote_ident);
      break;
    case REMOTE_USER:
      val.remote_user = new std::string(*other.val.remote_user);
      break;
    case REQUEST_METHOD:
      val.request_method = other.val.request_method;
      break;
    case SCRIPT_NAME:
      val.script_name = new std::list<std::string>(*other.val.script_name);
      break;
    case SERVER_NAME:
      val.server_name = new ServerName(*other.val.server_name);
      break;
    case SERVER_PORT:
      val.server_port = other.val.server_port;
      break;
    case SERVER_PROTOCOL:
      val.server_protocol = other.val.server_protocol;
      break;
    case SERVER_SOFTWARE:
      val.server_software = other.val.server_software;
      break;
    case X_:
      val.etc_val = new EtcMetaVar(*other.val.etc_val);
      break;
    }
  }
  
  CgiMetaVar& operator=(const CgiMetaVar &other) {
    if (this != &other) {
      // Clean up existing value
      switch (name) {
      case AUTH_TYPE:
        delete val.auth_type;
        break;
      case CONTENT_TYPE:
        delete val.content_type;
        break;
      case PATH_INFO:
        delete val.path_info;
        break;
      case PATH_TRANSLATED:
        delete val.path_translated;
        break;
      case QUERY_STRING:
        delete val.query_string;
        break;
      case REMOTE_HOST:
        delete val.remote_host;
        break;
      case REMOTE_IDENT:
        delete val.remote_ident;
        break;
      case REMOTE_USER:
        delete val.remote_user;
        break;
      case SCRIPT_NAME:
        delete val.script_name;
        break;
      case SERVER_NAME:
        delete val.server_name;
        break;
      case X_:
        delete val.etc_val;
        break;
      default:
        break;
      }
      
      // Copy new value
      name = other.name;
      switch (name) {
      case AUTH_TYPE:
        val.auth_type = new CgiAuthType(*other.val.auth_type);
        break;
      case CONTENT_LENGTH:
        val.content_length = other.val.content_length;
        break;
      case CONTENT_TYPE:
        val.content_type = new ContentType(*other.val.content_type);
        break;
      case GATEWAY_INTERFACE:
        val.gateway_interface = other.val.gateway_interface;
        break;
      case PATH_INFO:
        val.path_info = new std::list<std::string>(*other.val.path_info);
        break;
      case PATH_TRANSLATED:
        val.path_translated = new std::string(*other.val.path_translated);
        break;
      case QUERY_STRING:
        val.query_string = new std::map<std::string, std::string>(*other.val.query_string);
        break;
      case REMOTE_ADDR:
        val.remote_addr[0] = other.val.remote_addr[0];
        val.remote_addr[1] = other.val.remote_addr[1];
        val.remote_addr[2] = other.val.remote_addr[2];
        val.remote_addr[3] = other.val.remote_addr[3];
        break;
      case REMOTE_HOST:
        val.remote_host = new std::list<std::string>(*other.val.remote_host);
        break;
      case REMOTE_IDENT:
        val.remote_ident = new std::string(*other.val.remote_ident);
        break;
      case REMOTE_USER:
        val.remote_user = new std::string(*other.val.remote_user);
        break;
      case REQUEST_METHOD:
        val.request_method = other.val.request_method;
        break;
      case SCRIPT_NAME:
        val.script_name = new std::list<std::string>(*other.val.script_name);
        break;
      case SERVER_NAME:
        val.server_name = new ServerName(*other.val.server_name);
        break;
      case SERVER_PORT:
        val.server_port = other.val.server_port;
        break;
      case SERVER_PROTOCOL:
        val.server_protocol = other.val.server_protocol;
        break;
      case SERVER_SOFTWARE:
        val.server_software = other.val.server_software;
        break;
      case X_:
        val.etc_val = new EtcMetaVar(*other.val.etc_val);
        break;
      }
    }
    return *this;
  }
  
  ~CgiMetaVar() {
    switch (name) {
    case AUTH_TYPE:
      delete val.auth_type;
      break;
    case CONTENT_TYPE:
      delete val.content_type;
      break;
    case PATH_INFO:
      delete val.path_info;
      break;
    case PATH_TRANSLATED:
      delete val.path_translated;
      break;
    case QUERY_STRING:
      delete val.query_string;
      break;
    case REMOTE_HOST:
      delete val.remote_host;
      break;
    case REMOTE_IDENT:
      delete val.remote_ident;
      break;
    case REMOTE_USER:
      delete val.remote_user;
      break;
    case SCRIPT_NAME:
      delete val.script_name;
      break;
    case SERVER_NAME:
      delete val.server_name;
      break;
    case X_:
      delete val.etc_val;
      break;
    default:
      break;
    }
  }
  
  Name const &get_name() const { return name; }
  Val const &get_val() const { return val; }

private:
  Name name;
  Val val;
  CgiMetaVar(Name n, Val v) : name(n), val(v) {}
  static CgiMetaVar auth_type(CgiAuthType);
  static CgiMetaVar content_length(unsigned int);
  static CgiMetaVar content_type(ContentType);
  static CgiMetaVar gateway_interface(GatewayInterface);
  static CgiMetaVar path_info(std::list<std::string>);
  static CgiMetaVar path_translated(std::string);
  static CgiMetaVar query_string(std::map<std::string, std::string>);
  static CgiMetaVar remote_addr(unsigned char, unsigned char, unsigned char,
                                 unsigned char);
  static CgiMetaVar remote_host(std::list<std::string>);
  static CgiMetaVar remote_ident(std::string);
  static CgiMetaVar remote_user(std::string);
  static CgiMetaVar request_method(Http::Method);
  static CgiMetaVar script_name(std::list<std::string>);
  static CgiMetaVar server_name(ServerName);
  static CgiMetaVar server_port(unsigned short);
  static CgiMetaVar server_protocol(ServerProtocol);
  static CgiMetaVar server_software(ServerSoftware);
  static CgiMetaVar custom_var(EtcMetaVar::Type, std::string, std::string);
};

class CgiInput {
  std::vector<CgiMetaVar> mvars;
  Http::Body req_body;

private:
  CgiInput();
  CgiInput(std::vector<CgiMetaVar>, Http::Body);
  CgiInput(Http::Request const &);

public:
  class Parser {
    virtual void phantom() = 0;

  public:
    static Result<CgiInput> parse(Http::Request const &);
  };
  
  friend class Parser;
  friend class CgiDelegate;

  CgiInput(const CgiInput &other)
      : mvars(other.mvars), req_body(other.req_body) {}
  CgiInput& operator=(const CgiInput &other) {
    if (this != &other) {
      mvars = other.mvars;
      req_body = other.req_body;
    }
    return *this;
  }
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class CgiDelegate {
  CgiInput env;
  std::string script_path;
  Http::Request request;

  CgiDelegate(const Http::Request &req, const std::string &script);

public:
  static CgiDelegate *create(const Http::Request &req, const std::string &script);
  Result<Http::Response *> execute(int timeout_ms, EPoll *epoll);
  ~CgiDelegate();
};

unsigned char to_upper(unsigned char);

#endif
