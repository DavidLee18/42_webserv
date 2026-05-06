#ifndef RESPONSE_HPP
#define RESPONSE_HPP

/**
 * @file Response.hpp
 * @brief Defines the HTTP Response generation structures and classes.
 */

#include "../config/ServerConfig.hpp"
#include "Client.hpp"
#include "Session.hpp"

#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class EPoll;
class CgiDelegate;

/**
 * @struct Target
 * @brief Represents the resolved target resource path and its type.
 */
struct Target {
  std::string path; ///< The absolute path to the target resource.
  int type; ///< The type or status of the target (e.g., IS_DIR, IS_FILE,
            ///< errors).
};

/**
 * @struct StatusInfo
 * @brief Holds a status message and corresponding error file path.
 */
struct StatusInfo {
  std::string message;   ///< Status message.
  std::string file_path; ///< Path to the error file.
};

/**
 * @struct Response
 * @brief Represents the components of an HTTP response.
 */
struct Response {
  /**
   * @enum StatusCode
   * @brief Enum for commonly used HTTP status codes.
   */
  enum StatusCode {
    OK = 200,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
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
  std::string redir;     ///< Redirect location, if applicable.
  bool keep_alive;       ///< Connection keep-alive status. Whether to close
                         ///< the connection after sending a response.
  std::map<std::string, std::string>
      headers; ///< Additional response headers from config.

  Response()
      : version("HTTP/1.1"), status_code(INTERNAL_SERVER_ERR),
        content_length(0), content_type(), cookie(), body(), redir(),
        keep_alive(false), headers() {}
  static Result<Response> from_cgi_outbuff(std::string const &);
};

std::ostream &operator<<(std::ostream &, Response const &);

class Request;
class ServerConfig;

/**
 * @class ServerResponse
 * @brief Static utility class for generating HTTP responses.
 */
class ServerResponse {
public:
  static std::string find_file_type(const std::string &path);

  /**
   * @brief Determines MIME type based on file extension.
   *
   * @param ext File extension (without dot).
   * @return MIME type string (defaults to "text/html" for unknown types).
   */
  static std::string get_mime_type_for_extension(const std::string &ext);

  /**
   * @brief Generates an Response based on the client request and server
   * configuration.
   *
   * @param request Pointer to the parsed Request object.
   * @param client
   * @param mime_type Map of file extension to MIME types.
   * @param session
   * @return Response The fully formulated HTTP response components.
   */
  static Response
  http_response(const Request *request, const ClientSession *client,
                const std::map<std::string, std::string> &mime_type,
                Session *session);

  static Result<CgiDelegate>
  register_cgi(const Request &request, const RouteRule_CGI &rule, EPoll *epoll);

private:
  /**
   * @enum Type
   * @brief Enum for internal target path typing.
   */
  enum Type { IS_DIR, IS_FILE, PATH_ERROR };

  /**
   * @brief Checks the file system to determine what kind of resource exists at
   * the given path.
   *
   * @param path The file system path to check.
   * @return int The determined Status/Type (e.g., IS_DIR, IS_FILE, or error
   * codes).
   */
  static int check_path_type(const std::string &path);

  /**
   * @brief Resolves the final target resource according to matching RouteRule.
   *
   * @param rule Pointer to the matched RouteRule.
   * @param config Pointer to the relevant ServerConfig.
   * @param request Pointer to the Request object.
   * @return Target The resolved target path and its calculated type/status.
   */
  static Target resolve_target(const RouteRule *rule,
                               const ServerConfig *config,
                               const Request *request);

  /**
   * @brief Retreives the current working directory of the process.
   *
   * @return std::string The absolute path of the current working directory.
   */
  static std::string get_pwd();

  /**
   * @brief Gets the custom error file path corresponding to a status code.
   *
   * @param config
   * @param rule
   * @param error_code The HTTP error status code.
   * @return std::string Path to the configured error file.
   */
  static Response
  error_response(const ServerConfig *config, const RouteRule *rule,
                 Response::StatusCode error_code,
                 const std::map<std::string, std::string> &mime_type);

  /**
   * @brief Generates an HTML page listing the contents of a directory
   * (autoindex).
   *
   * @param real_path The physical directory path on the local file system.
   * @param req_uri The request URI path used by the client.
   * @param dir
   * @return std::string The HTML content representing the directory index.
   */
  static std::string make_autoindex_page(const std::string &real_path,
                                         const std::string &req_uri, DIR *dir);

  /**
   * @brief Extract boundary string from Content-Type header for multipart
   * requests.
   * @param content_type The Content-Type header value
   * @return The boundary string (without -- prefix), or empty string if not
   * multipart
   */
  static std::string extract_boundary(const std::string &content_type);

  /**
   * @brief Parse a single part from multipart form data body
   * @param body The request body
   * @param boundary The boundary marker
   * @param start_pos Starting position in body to search from
   * @param out_filename Reference to store extracted filename (empty if not a
   * file field)
   * @param out_fieldname Reference to store extracted field name
   * @param out_data Reference to store the part body data
   * @return Position of next part boundary, or string::npos if no more parts
   */
  static std::size_t
  parse_multipart_part(const std::string &body, const std::string &boundary,
                       std::size_t start_pos, std::string &out_filename,
                       std::string &out_fieldname, std::string &out_data);

  static Response
  delete_method(const Target &target, Response response,
                const ServerConfig *config, const RouteRule *rule,
                const std::map<std::string, std::string> &mime_type);
  static Response
  post_method(const Target &target, Response response,
              const ClientSession *client, const RouteRule *rule,
              const Request *request, Session *session,
              const std::map<std::string, std::string> &mime_type);
  static Response
  get_method(Target target, Response response, const ServerConfig *config,
             const RouteRule *rule, const Request *request,
             const std::map<std::string, std::string> &mime_type);
};

class DefaultError {
  virtual int phantom() = 0;
  static std::string bad_request();
  static std::string forbidden();
  static std::string not_found();
  static std::string server_error();
  static std::string unknown_err();

public:
  static Response::StatusCode int_to_status_code(unsigned short status_code);
  static std::string status_code_to_string(Response::StatusCode status_code);
  static Response default_err_response(Response::StatusCode err_code);
};

#endif
