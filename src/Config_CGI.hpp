#ifndef CONFIG_CGI_HPP
#define CONFIG_CGI_HPP

#include "ParsingUtils.hpp"
#include "file_descriptor.h"
#include "http_1_1.h"
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

class Config_CGI {
private:
  std::string executable;
  std::map<std::string, std::string> env;
  double timeout;
  std::string err;

  std::string parse_CGI(FileDescriptor &fd, std::string line);

public:
  Config_CGI() : executable(""), env(), timeout(-1), err("No parse"){};
  Config_CGI(FileDescriptor &fd, std::string);

  const std::string Get_err() const { return err; }
  const std::string Get_executable() const { return executable; }
  const std::map<std::string, std::string> Get_env() const { return env; }
  double Get_timeout() const { return timeout; }
};

std::string parse_Config_uwsgi(FileDescriptor &fd,
                        std::map<std::string, std::string> &uwsgi);
bool is_Config_CGI(std::string line);
bool is_CGI(const std::string &line);
std::ostream &operator<<(std::ostream &os, const Config_CGI &data);
bool isExecutableFile(const std::string &path);
std::string parse_env(const std::string &, std::map<std::string, std::string> &env);

#endif