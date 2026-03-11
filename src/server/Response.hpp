#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "../ServerConfig.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

struct Path {
  std::string route;
  int type;
};

struct StatusInfo {
  std::string message;
  std::string file_path;
};

struct HttpResponse {
  std::string version;
  std::string status_code;
  std::string body;
  std::string file_type;
  bool keep_alive;
};

class Response {
public:
  static HttpResponse generate(const Http::Request *request,
                               const ServerConfig *config);

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

  static const std::map<int, std::string> status_code;
  static std::map<int, std::string> init_status_code();

  static int check_path_type(const std::string &path);
  static Path resolve_path(const Http::Request *request,
                                  const ServerConfig *config);
  static std::string get_pwd();
  static std::string error_file_path(int error_code);
};

#endif