#ifndef CGI_1_1_H
#define CGI_1_1_H

#include "WebserverConfig.hpp"
#include "http_1_1.h"
#include "result.h"
#include <list>
#include <map>
#include <string>

enum CgiMetaVarName {
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

enum CgiAuthType {
  Basic,
  Digest,
  CgiAuthOther,
};

class ContentType {
  std::string type;
  std::string subtype;
  std::map<std::string, std::string> params;

public:
  ContentType(std::string ty, std::string subty)
      : type(ty), subtype(subty), params() {}
  ContentType(ContentType const &other)
      : type(other.type), subtype(other.subtype), params(other.params) {}
  Result<Void> add_param(std::string, std::string);
};

enum GatewayInterface { Cgi_1_1 };

class ServerName {
  enum ServerNameType {
    Host,
    Ipv4,
  } type;
  union ServerNameVal {
    std::list<std::string> *host_name;
    unsigned char ipv4[4];
  } val;

  ServerName(ServerNameType ty, ServerNameVal v) : type(ty), val(v) {}

  static ServerName *host(std::list<std::string>);
  static ServerName *ipv4(unsigned char, unsigned char, unsigned char,
                          unsigned char);

public:
  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<ServerName *, size_t> > parse_host(std::string);
    static Result<std::pair<ServerName *, size_t> > parse_ipv4(std::string);

  public:
    static Result<std::pair<ServerName *, size_t> > parse(std::string);
  };
};

enum ServerProtocol { Http_1_1 };

enum ServerSoftware { Webserv };

enum EtcMetaVarType { Http, Custom };

class EtcMetaVar {
  EtcMetaVarType type;
  std::string name;
  std::string value;

public:
  EtcMetaVar(EtcMetaVarType ty, std::string n, std::string v)
      : type(ty), name(n), value(v) {}
  EtcMetaVar(EtcMetaVar const &other)
      : type(other.type), name(other.name), value(other.value) {}
};

union CgiMetaVar_ {
  CgiAuthType auth_type;
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
  HttpMethod request_method;
  std::list<std::string> *script_name;
  ServerName *server_name;
  unsigned short server_port;
  ServerProtocol server_protocol;
  ServerSoftware server_software;
  EtcMetaVar *etc_val;
};
class CgiMetaVar {
  CgiMetaVarName name;
  CgiMetaVar_ val;
  CgiMetaVar(CgiMetaVarName n, CgiMetaVar_ v) : name(n), val(v) {}
  static CgiMetaVar *auth_type(CgiAuthType);
  static CgiMetaVar *content_length(unsigned int);
  static CgiMetaVar *content_type(ContentType);
  static CgiMetaVar *gateway_interface(GatewayInterface);
  static CgiMetaVar *path_info(std::list<std::string>);
  static CgiMetaVar *path_translated(std::string);
  static CgiMetaVar *query_string(std::map<std::string, std::string>);
  static CgiMetaVar *remote_addr(unsigned char, unsigned char, unsigned char,
                                 unsigned char);
  static CgiMetaVar *remote_host(std::list<std::string>);
  static CgiMetaVar *remote_ident(std::string);
  static CgiMetaVar *remote_user(std::string);
  static CgiMetaVar *request_method(HttpMethod);
  static CgiMetaVar *script_name(std::list<std::string>);
  static CgiMetaVar *server_name(ServerName *);
  static CgiMetaVar *server_port(unsigned short);
  static CgiMetaVar *server_protocol(ServerProtocol);
  static CgiMetaVar *server_software(ServerSoftware);
  static CgiMetaVar *custom_var(EtcMetaVarType, std::string, std::string);

public:
  class Parser {
    virtual void phantom() = 0;
    static Result<std::pair<CgiMetaVar *, size_t> > parse_auth_type(std::string);
  };
};

class CgiInput {
  std::vector<CgiMetaVar> mvars;
  HttpBody req_body;

public:
  CgiInput(HttpReq const &);
  CgiInput(const CgiInput &other)
      : mvars(other.mvars), req_body(other.req_body) {}
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class CgiDelegate {
  CgiInput env;
  CgiDelegate(HttpReq req) : env(req) {}

public:
  ~CgiDelegate();
};

unsigned char to_upper(unsigned char c) {
  return static_cast<unsigned char>(std::tolower(static_cast<int>(c)));
}

#endif
