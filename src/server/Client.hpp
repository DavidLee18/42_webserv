#ifndef CLIENT_HPP
#define CLIENT_HPP

/**
 * @file Client.hpp
 * @brief Defines the ClientSession struct and Request class for handling client HTTP requests.
 */

#include <map>
#include <cctype>
#include <sstream>
#include <string>
#include <iostream>
#include <stdlib.h>

/**
 * @brief Utility function to retrieve a value from a map of strings based on a key.
 * 
 * @param map The map to search in.
 * @param key The key to look for.
 * @return std::string The value corresponding to the key, or an empty string if not found.
 */
std::string get_string_from_map(const std::map<std::string, std::string> map, std::string key);

class ServerConfig;

/**
 * @struct ClientSession
 * @brief Holds information and buffers for a single client session.
 */
struct ClientSession {
  std::string in_buff;  ///< Buffer for incoming data.
  std::string out_buff; ///< Buffer for outgoing data.

  const ServerConfig *config; ///< Pointer to the server configuration for this session.
  std::string cookie;
  std::string ip;

  /**
   * @brief Default constructor. Initializes config to NULL.
   */
  ClientSession() : config(NULL) {}
};

/**
 * @class Request
 * @brief Parses and stores information from an HTTP request.
 */
class Request {
private:
  std::string method;       ///< The HTTP method (e.g., "GET").
  std::string path;         ///< The requested path (e.g., "/index.html").
  std::string version;      ///< The HTTP version (e.g., "HTTP/1.1").
  std::map<std::string, std::string> header; ///< Parsed HTTP headers.
  bool keep_alive;          ///< Connection keep-alive status.
  std::string cookie;
  std::string body;         ///< The request body, if any.
  bool content_full;        ///< Flag indicating if the entire body has been received.

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

  /**
   * @brief Construct a new Request object by parsing an HTTP request string.
   * 
   * @param request The raw HTTP request string.
   */
  Request(std::string request);

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
  Request::Method get_method() const;

  /**
   * @brief Gets the requested HTTP method as a string.
   * 
   * @return const std::string The method string.
   */
  const std::string get_method_string() const { return method; };

  /**
   * @brief Gets the requested path.
   * 
   * @return const std::string The requested path.
   */
  const std::string get_path() const { return path; };

  /**
   * @brief Gets client's cookie.
   * 
   * @return const std::string The client's cookie. Generate default cookie if none.
   */
  const std::string get_cookie() const { return cookie; };

  /**
   * @brief Sets client's cookie.
   */
  void set_cookie(std::string value) { cookie = value; };

  /**
   * @brief Gets the parsed HTTP headers.
   * 
   * @return const std::map<std::string, std::string>& The headers map.
   */
  const std::map<std::string, std::string>& get_headers() const { return header; }

  /**
   * @brief Gets the request body.
   * 
   * @return const std::string& The request body.
   */
  const std::string& get_body() const { return body; }
};

#endif
