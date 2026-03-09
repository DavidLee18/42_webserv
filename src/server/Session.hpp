#ifndef SESSION_HPP
#define SESSION_HPP

#include "../ServerConfig.hpp"
#include <string>

struct ClientSession {
  std::string in_buff;
  std::string out_buff;

  const ServerConfig *config;

  ClientSession() : config(NULL) {}
};

#endif