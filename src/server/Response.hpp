#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "../ServerConfig.hpp"
#include "Client.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

struct Target {
  std::string path;
  int type;
};

struct StatusInfo {
  std::string message;
  std::string file_path;
};

struct HttpResponse {
  std::string version;
  std::string status_code;
  std::string content_type;
  std::string connection;
  std::string body;
  std::string mime_type;
  bool keep_alive;
};

class Request;
class ServerConfig;
class Response {
public:
  static HttpResponse generate(const Request *request,
                               const ServerConfig *config,
                               const std::map<std::string, std::string>mime_type);

private:
  enum Type { IS_DIR, IS_FILE, PATH_ERROR };

  enum StatusCode {
    OK = 200,
    MOVED_PERMANENTLY = 301,
    BAD_REQUEST = 400,
    FORBIDDEN_ERR = 403,
    NOT_FOUND_ERR = 404,
    METHOD_NOT_ALLOWED = 405,
    PAYLOAD_TOO_LARGE = 413,
    INTERNAL_SERVER_ERR = 500
  };

  static std::string status_code_to_string(int status_code);

  static int check_path_type(const std::string &path);
  static Target resolve_target(const RouteRule *rule, const ServerConfig *config, const Request *request);
  static std::string get_pwd();
  static std::string make_autoindex_page(const std::string& real_path,
                                         const std::string& req_uri);
};

#endif