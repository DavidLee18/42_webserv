#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "../config/ServerConfig.hpp"
#include "../cgi_1_1/CgiDelegate.hpp"
#include "../utils/Errors.hpp"
#include "../utils/utils.hpp"

#include "Client.hpp"
#include "Session.hpp"

#include <algorithm>
#include <cerrno>
#include <ctime>
#include <sys/stat.h>
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
  int type; ///< The type or status of the target.
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
  from_cgi_outbuff(std::string const &,
                   std::map<std::string, std::string> const &);
  void print_simple(std::ostream &) const;
};

std::ostream &operator<<(std::ostream &, Response const &);

class Request;
class ServerConfig;

class ServerResponse {
public:
  static std::string find_file_type(const std::string &path);
  static std::string get_mime_type_for_extension(const std::string &ext);
  static Response http_response(
      const Request *request, const ClientSession *client,
      const std::map<std::string, std::string> &mime_type,
      Session *session, char **envp);
  static Result<Void> register_cgi(
      const Request &request, const RouteRule_CGI &rule, EPoll *epoll,
      std::map<std::string, std::string> const &cgi_interpreters,
      std::map<FileDescriptor const *,
               std::pair<FileDescriptor const *, CgiDelegate *> > &cgis,
      FileDescriptor const *client_fd, char **envp);

private:
  enum PathType { IS_DIR, IS_FILE, PATH_ERROR };
  static Result<int> check_path_type(const std::string &path);
  static Target resolve_target(const RouteRule *rule,
                               const ServerConfig *config,
                               const Request *request, char **envp);
  static Result<std::string> compute_etag(const std::string &path);
  static Result<std::string> get_last_modified(const std::string &path);
  static Response error_response(const ServerConfig *config,
                                 const RouteRule *rule,
                                 Response::StatusCode error_code, char **envp);
  static Result<std::string> make_autoindex_page(const std::string &real_path,
                                                 const std::string &req_uri, DIR *dir);
  static std::string extract_boundary(const std::string &content_type);
  static std::size_t parse_multipart_part(
                       const std::string &body, const std::string &boundary,
                       std::size_t start_pos, std::string &out_filename,
                       std::string &out_fieldname, std::string &out_data);

  static Response delete_method(const Target &target, Response response,
                                const ServerConfig *config,
                                const RouteRule *rule, char **envp);
  static Response post_method(const Target &target, Response response,
                              const ClientSession *client,
                              const RouteRule *rule, const Request *request,
                              Session *session, char **envp);
  static Response get_method(Target target, Response response,
                             const ServerConfig *config, const RouteRule *rule,
                             const Request *request, char **envp);
};

class DefaultError {
  virtual int phantom() = 0;

public:
  static Response::StatusCode int_to_status_code(unsigned short status_code);
  static std::string status_code_to_string(Response::StatusCode status_code);
  static Response default_err_response(Response::StatusCode err_code);
};

#endif
