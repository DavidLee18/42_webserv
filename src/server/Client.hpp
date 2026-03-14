#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Server.hpp"
#include <sstream>
#include <string>

class ServerConfig;
struct ClientSession {
  std::string in_buff;
  std::string out_buff;

  const ServerConfig *config;

  ClientSession() : config(NULL) {}
};

class Request {
private:
  std::string method;
  std::string path;
  std::string version;
  bool keep_alive;
  std::map<std::string, std::string> header;
  std::string body;

public:
  enum Method { GET, HEAD, OPTIONS, POST, DELETE, PUT, CONNECT, TRACE, PATCH };
  Request(std::string request);
  const std::string get_connection_string() const;
  Request::Method get_method() const;
  const std::string get_method_string() const { return method; };
  const std::string get_path() const { return path; };
};

#endif
