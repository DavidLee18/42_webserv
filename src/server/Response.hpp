#ifndef RESPONSE_HPP
#define RESPONSE_HPP

/**
 * @file Response.hpp
 * @brief Defines the HTTP Response generation structures and classes.
 */

#include "../config/ServerConfig.hpp"
#include "Client.hpp"
#include "DefaultError.hpp"
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class EPoll;

/**
 * @enum StatusCode
 * @brief Enum for commonly used HTTP status codes.
 */
enum StatusCode {
  OK = 200,
  MOVED_PERMANENTLY = 301,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN_ERR = 403,
  NOT_FOUND_ERR = 404,
  METHOD_NOT_ALLOWED = 405,
  CONFLICT = 409,
  PAYLOAD_TOO_LARGE = 413,
  INTERNAL_SERVER_ERR = 500
};

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
 * @brief Holds status message and corresponding error file path.
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
  std::string version;      ///< HTTP version (e.g., "HTTP/1.1").
  std::string status_code;  ///< HTTP status code and reason (e.g., "200 OK").
  std::string content_type; ///< Content-Type header.
  std::string connection;   ///< Connection header.
  std::string cookie;       ///< Cookies.
  std::string body;         ///< The response body payload.
  std::string mime_type; ///< The determined MIME type of the response payload.
  std::string redir;     ///< Redirect location, if applicable.
  bool keep_alive;       ///< Connection keep-alive status.
  std::string cgi;       ///< Generated CGI script.
};

class Request;
class ServerConfig;

/**
 * @class Response
 * @brief Static utility class for generating HTTP responses.
 */
class ServerResponse {
public:
  static std::string find_file_type(std::string path);
  /**
   * @brief Generates an Response based on the client request and server
   * configuration.
   *
   * @param request Pointer to the parsed Request object.
   * @param config Pointer to the ServerConfig for the target server.
   * @param mime_type Map of file extension to MIME types.
   * @return Response The fully formulated HTTP response components.
   */
  static Response
  http_response(const Request *request, const ServerConfig *config,
                const std::map<std::string, std::string> mime_type);

  static Response cgi_response(const Request *request,
                               const ServerConfig *config, EPoll *epoll);

private:
  /**
   * @enum Type
   * @brief Enum for internal target path typing.
   */
  enum Type { IS_DIR, IS_FILE, PATH_ERROR };

  /**
   * @brief Converts an integer status code to its HTTP reason phrase string.
   *
   * @param status_code The numeric HTTP status code.
   * @return std::string The status line string (e.g., "200 OK").
   */
  static std::string status_code_to_string(int status_code);

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
   * @param error_code The HTTP error status code.
   * @return std::string Path to the configured error file.
   */
  static Response error_response(const ServerConfig *config,
                                 const RouteRule *rule, int error_code);

  /**
   * @brief Generates an HTML page listing the contents of a directory
   * (autoindex).
   *
   * @param real_path The physical directory path on the local file system.
   * @param req_uri The request URI path used by the client.
   * @return std::string The HTML content representing the directory index.
   */
  static std::string make_autoindex_page(const std::string &real_path,
                                         const std::string &req_uri, DIR *dir);

  static Response delete_method(Target target, Response response,
                                const ServerConfig *config,
                                const RouteRule *rule);
  static Response post_method(Target target, Response response,
                              const ServerConfig *config,
                              const RouteRule *rule,
                              const Request *request);
  static Response get_method(Target target, Response response,
                             const ServerConfig *config,
                             const RouteRule *rule,
                             const Request *request);
};

#endif