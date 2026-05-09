#ifndef CLIENT_HPP
#define CLIENT_HPP

/**
 * @file Client.hpp
 * @brief Defines the ClientSession struct and Request class for handling client
 * HTTP requests.
 */
#include "../result.h"
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <sstream>
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
std::string get_string_from_map(const std::map<std::string, std::string> &map,
                                std::string const &key);

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
  timespec
      last_activity_time; ///< Timestamp of last activity for timeout tracking.
  bool dropping;

  /**
   * @brief Default constructor. Initializes config to NULL.
   */
  ClientSession()
      : config(NULL), req(NULL), last_activity_time(), dropping(false) {}
  ~ClientSession();
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

  enum UnchunkState { NOT_CHUNKED, READING, DONE };

  static Result<Request *> from_buff(std::string &);

  /**
   * @brief Gets the value of the Connection header.
   *
   * @return const std::string Connection header value.
   */
  std::string get_connection_string() const;

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
  std::string get_path() const { return path; }

  /**
   * @brief Gets client's cookie.
   *
   * @return const std::string The client's cookie. Generate default cookie if
   * none.
   */
  std::string get_cookie() const { return cookie; }

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
  void set_cookie(const std::string &value) { cookie = value; }

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

  Result<size_t> get_content_length() const;

  bool is_partial() const;

  bool is_chunked() const;

  bool has_keep_alive() const { return keep_alive; }

  Result<Void> continue_parsing(std::string &);

  Result<size_t> unchunk(size_t remnant_end);

private:
  Method method;       ///< The HTTP method (e.g., "GET").
  std::string path;    ///< The requested path (e.g., "/").
  std::string version; ///< The HTTP version (e.g., "HTTP/1.1").
  std::map<std::string, std::string> header; ///< Parsed HTTP headers.
  bool keep_alive; ///< Connection keep-alive status (HTTP/1.1 default: true).
  ssize_t content_length;
  std::string cookie;
  std::string body;     ///< The request body, if any.
  std::string remnants; ///< remaining string to parse.
  UnchunkState decode_chunk_state;

  Request()
      : method(ERROR), path(), version(), header(), keep_alive(true),
        content_length(-1), cookie(), body(), remnants(),
        decode_chunk_state(NOT_CHUNKED) {}
  Request(const Method method, std::string const &path,
          std::string const &version, const size_t content_length)
      : method(method), path(path), version(version), keep_alive(true),
        content_length(static_cast<ssize_t>(content_length)),
        decode_chunk_state(NOT_CHUNKED) {}
  Request(const Method method, std::string const &path,
          std::string const &version)
      : method(method), path(path), version(version), keep_alive(true),
        content_length(0), decode_chunk_state(READING) {}
};

#endif
