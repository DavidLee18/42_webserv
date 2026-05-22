#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../core/Errors.hpp"
#include "../core/Result.hpp"

#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

class ServerConfig;
class Request;

struct ClientSession {
    std::string in_buff;  ///< Buffer for incoming data.
    std::string out_buff; ///< Buffer for outgoing data.

  const ServerConfig *config; ///< Pointer to the server configuration.
  std::string cookie;
  std::string ip;
  Request *req;
  timespec last_activity_time; ///< Timestamp for timeout tracking.
  bool dropping;
  std::string out_file_path; ///< File path for streamed responses.
  size_t out_file_offset;    ///< Offset into streaming file.
  bool streaming_file;       ///< Whether we're currently streaming a file.

  ClientSession()
      : config(NULL), req(NULL), last_activity_time(), dropping(false),
        out_file_path(), out_file_offset(0), streaming_file(false) {}
  ~ClientSession();
};

// Class to parse and store information from an HTTP request.
class Request {
public:
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

  Request(Request const &other)
      : method(other.method), path(other.path), version(other.version),
        header(other.header), keep_alive(other.keep_alive),
        content_length(other.content_length), cookie(other.cookie),
        body(other.body), remnants(other.remnants),
        decode_chunk_state(other.decode_chunk_state) {}

  static Result<Request *> from_buff(std::string &);
  std::string get_connection_string() const;
  Request::Method get_method() const { return method; }
  std::string get_method_string() const;
  std::string get_path() const { return path; }
  std::string get_cookie() const { return cookie; }
  std::string get_cookie_value(const std::string &name) const;
  void set_cookie(const std::string &value) { cookie = value; }
  const std::map<std::string, std::string> &get_headers() const {
    return header;
  }
  const std::string &get_query() const { return query; }
  const std::string &get_body() const { return body; }
  Result<size_t> get_content_length() const;

    bool               is_partial() const;

    bool               is_chunked() const;

    bool               has_keep_alive() const { return keep_alive; }

    Result<Void>       continue_parsing(std::string&);

    Result<size_t>     unchunk(size_t remnant_end);

  // Helper parsers broken out from from_buff for clarity and testability.
  static Result<Void> parse_request_line(std::stringstream &ss, Method &method,
                                         std::string &req_path,
                                         std::string &req_version,
                                         const size_t cl_pos,
                                         const size_t te_pos,
                                         std::string &line);

  static Result<Void> parse_headers(std::stringstream &ss, Request *req);

  static Result<Void> parse_body_or_chunked(std::stringstream &ss,
                                            std::string &buff, Request *req,
                                            const size_t header_end);

private:
  Method method;       ///< The HTTP method (e.g., "GET").
  std::string path;    ///< The requested path (e.g., "/").
  std::string version; ///< The HTTP version (e.g., "HTTP/1.1").
  std::string query;
  std::map<std::string, std::string> header; ///< Parsed HTTP headers.
  bool keep_alive; ///< Connection keep-alive status (HTTP/1.1 default: true).
  ssize_t content_length;
  std::string cookie;
  std::string body;     ///< The request body, if any.
  std::string remnants; ///< remaining string to parse.
  UnchunkState decode_chunk_state;

    Request()
        : method(ERROR), path(), version(), query(), header(), keep_alive(true),
          content_length(-1), cookie(), body(), remnants(),
          decode_chunk_state(NOT_CHUNKED) {}
    Request(const Method method, std::string const& path,
            std::string const& version, const size_t content_length)
        : method(method), path(path), version(version), keep_alive(true),
          content_length(static_cast<ssize_t>(content_length)),
          decode_chunk_state(NOT_CHUNKED) {
        const size_t query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            query      = path.substr(query_pos + 1);
            this->path = path.substr(0, query_pos);
        }
    }
    Request(const Method method, std::string const& path,
            std::string const& version)
        : method(method), path(path), version(version), keep_alive(true),
          content_length(-1), decode_chunk_state(READING) {
        const size_t query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            query      = path.substr(query_pos + 1);
            this->path = path.substr(0, query_pos);
        }
    }
};

std::string get_string_from_map(const std::map<std::string, std::string> &map,
                                std::string const &key);

#endif
