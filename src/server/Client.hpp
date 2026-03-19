#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <map>
#include <sstream>
#include <string>

std::string get_string_from_map(const std::map<std::string, std::string> map, std::string key);

class ServerConfig;
struct ClientSession {
  std::string in_buff;
  std::string out_buff;

  const ServerConfig *config;

  ClientSession() : config(NULL) {}
};

class Request {
private:
  std::string method; // "GET"
  std::string path; // "/index.html"
  std::string version; // "HTTP/1.1"
  bool keep_alive; //
  std::map<std::string, std::string> header;
  std::string body;
  bool content_full;

public:
  enum Method { GET, HEAD, OPTIONS, POST, DELETE, PUT, CONNECT, TRACE, PATCH, ERROR };
  Request(std::string request);
  const std::string get_connection_string() const;
  Request::Method get_method() const;
  const std::string get_method_string() const { return method; };
  const std::string get_path() const { return path; };
  const std::string get_path_only() const;
};

#endif
