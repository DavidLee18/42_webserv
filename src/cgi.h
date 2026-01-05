#ifndef CGI_H
#define CGI_H

#include "result.h"
#include <map>
#include <string>

class CgiEnv {
  std::map<std::string, std::string> mvars;

public:
  CgiEnv() : mvars() {}
  CgiEnv(const CgiEnv &other) : mvars(other.mvars) {}
  void add_mvar(std::string key, std::string value);
  char **to_envp();
};

class Cgi {};

#endif
