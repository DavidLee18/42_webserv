#ifndef CGI_1_1_H
#define CGI_1_1_H

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
  Token_,
};

struct ContentType {
  std::string type;
  std::string subtype;

  ContentType(std::string ty, std::string subty)
      : type(ty), subtype(subty), params() {}
  void add_param(std::string, std::string);

private:
  std::map<std::string, std::string> params;
};

enum GatewayInterface { Cgi_1_1 };

enum ServerNameType {
  Host,
  Ipv4,
};

struct ServerName {
  ServerNameType type;
  union ServerNameVal {
    std::list<std::string> *host_name;
    unsigned char ipv4[4];
  } val;
};

enum ServerProtocol { Http_1_1 };

enum ServerSoftware { Webserv };

enum EtcMetaVarType { Http, Custom };

struct CgiMetaVar {
  CgiMetaVarName name;
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
    ServerName server_name;
    unsigned short server_port;
    ServerProtocol server_protocol;
    ServerSoftware server_software;
    struct EtcMetaVar {
      EtcMetaVarType type;
      std::string name;
      void *val;
    } *etc_val;
  } val;
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

#endif
