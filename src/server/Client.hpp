#ifndef CLIENT_HPP
#define CLIENT_HPP

/**
 * @file Client.hpp
 * @brief Defines the ClientSession struct and Request class for handling client
 * HTTP requests.
 */
#include "../result.h"
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <string>

/**
 * @brief Utility function to retrieve a value from a map of strings based on a
 * key.
 *
 * @param map The map to search in.
 * @param key The key to look for.
 * @return std::string The value corresponding to the key, or an empty string if
 * not found.
 */
std::string get_string_from_map(const std::map<std::string, std::string> map,
                                std::string key);

class ServerConfig;
class Request;

/**
 * @struct ClientSession
 * @brief Holds information and buffers for a single client session.
 */
struct ClientSession {
  std::string in_buff;  ///< Buffer for incoming data.
  std::string out_buff; ///< Buffer for outgoing data.

  const ServerConfig
      *config; ///< Pointer to the server configuration for this session.
  std::string cookie;
  std::string ip;
  Request *req;

  /**
   * @brief Default constructor. Initializes config to NULL.
   */
  ClientSession() : config(NULL), req(NULL) {}
};

/**
 * @class Request
 * @brief Parses and stores information from an HTTP request.
 */
class Request {
public:
  /**
   * @enum Method
   * @brief Enum representing standard HTTP methods.
   */
  enum Method {
    GET,
    HEAD,
    OPTIONS,
    POST,
    DELETE,
    PUT,
    CONNECT,
    TRACE,
    PATCH,
    ERROR
  };

  static Result<Request *> from_buff(std::string &);

  /**
   * @brief Gets the value of the Connection header.
   *
   * @return const std::string Connection header value.
   */
  const std::string get_connection_string() const;

  /**
   * @brief Gets the requested HTTP method as an enum value.
   *
   * @return Request::Method The method enum.
   */
  Request::Method get_method() const { return method; }

  /**
   * @brief Gets the requested HTTP method as a string.
   *
   * @return const std::string The method string.
   */
  std::string get_method_string() const;

  /**
   * @brief Gets the requested path.
   *
   * @return const std::string The requested path.
   */
  const std::string get_path() const { return path; }

  /**
   * @brief Gets client's cookie.
   *
   * @return const std::string The client's cookie. Generate default cookie if
   * none.
   */
  const std::string get_cookie() const { return cookie; }

  /**
   * @brief Parses the cookie string and returns the value of a specific cookie
   * by name.
   *
   * @param name The name of the cookie to find (e.g., "session_id")
   * @return std::string The value of the cookie, or empty string if not found.
   */
  std::string get_cookie_value(const std::string &name) const;

  /**
   * @brief Sets client's cookie.
   */
  void set_cookie(std::string value) { cookie = value; }

  /**
   * @brief Gets the parsed HTTP headers.
   *
   * @return const std::map<std::string, std::string>& The headers map.
   */
  const std::map<std::string, std::string> &get_headers() const {
    return header;
  }

  /**
   * @brief Gets the request body.
   *
   * @return const std::string& The request body.
   */
  const std::string &get_body() const { return body; }

  size_t get_content_length() const { return content_length; }

  bool is_partial() const { return !remnants.empty(); }

  void continue_parsing(std::string &);

private:
  Method method;       ///< The HTTP method (e.g., "GET").
  std::string path;    ///< The requested path (e.g., "/").
  std::string version; ///< The HTTP version (e.g., "HTTP/1.1").
  std::map<std::string, std::string> header; ///< Parsed HTTP headers.
  bool keep_alive;                           ///< Connection keep-alive status.
  size_t content_length;
  std::string cookie;
  std::string body;     ///< The request body, if any.
  std::string remnants; ///< remaining string to parse.

  Request()
      : method(ERROR), path(), version(), header(), keep_alive(false),
        content_length(0), cookie(), body(), remnants() {}
  Request(Method method, std::string const &path, std::string const &version,
          size_t content_length)
      : method(method), path(path), version(version),
        content_length(content_length) {}
};

#endif
