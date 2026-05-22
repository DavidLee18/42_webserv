#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "../cgi_1_1/CgiDelegate.hpp"
#include "../config/ServerConfig.hpp"
#include "../core/Errors.hpp"
#include "../core/utils.hpp"

#include "Client.hpp"
#include "Session.hpp"

#include <algorithm>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class EPoll;
class CgiDelegate;

struct Target {
  std::string path; ///< The absolute path to the target resource.
  int type;         ///< The type or status of the target.
};

struct StatusInfo {
    std::string message;   ///< Status message.
    std::string file_path; ///< Path to the error file.
};

struct Response {
  enum StatusCode {
    OK = 200,
    NO_CONTENT = 204,
    NOT_MODIFIED = 304,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    PAYLOAD_TOO_LARGE = 413,
    INTERNAL_SERVER_ERR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    GATEWAY_TIMEOUT = 504,
  };
  std::string version;      ///< HTTP version (e.g., "HTTP/1.1").
  StatusCode status_code;   ///< HTTP status code and reason (e.g., "200 OK").
  size_t content_length;    ///< Content-Length header value.
  std::string content_type; ///< Content-Type header.
  std::string cookie;       ///< Cookies.
  std::string body;         ///< The response body payload.
  std::string file_path;    ///< If set, stream body from this file path.
  std::string redir;        ///< Redirect location, if applicable.
  bool keep_alive;          ///< Connection keep-alive status.
  std::map<std::string, std::string> headers;

    Response()
        : version("HTTP/1.1"), status_code(INTERNAL_SERVER_ERR),
          content_length(0), content_type(), cookie(), body(), redir(),
          keep_alive(false), headers() {}
    static Result<Response>
         from_cgi_outbuff(std::string const&,
                          std::map<std::string, std::string> const&);
    void print_simple(std::ostream&) const;
};

std::ostream& operator<<(std::ostream&, Response const&);

class Request;
class ServerConfig;

enum PathType { IS_DIR, IS_FILE, PATH_ERROR };

#endif
