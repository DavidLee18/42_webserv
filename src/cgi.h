#ifndef CGI_H
#define CGI_H

#include "WebserverConfig.hpp"
#include "http_1_1.h"
#include "result.h"
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
};

class CgiEnv {
  std::map<std::string, std::string> mvars;

public:
  CgiEnv() : mvars() {}
  CgiEnv(const CgiEnv &other) : mvars(other.mvars) {}
  void add_mvar(std::string const &, std::string const &);
  char **to_envp() const;
};

class CgiDelegate {
  CgiEnv env;
  CgiDelegate() : env() {}

public:
  ~CgiDelegate();
  static Result<CgiDelegate> from_req(const HttpReq &);
};

#endif
