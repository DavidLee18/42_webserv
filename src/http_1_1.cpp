#include "webserv.h"

// Helper function to skip whitespace
static size_t skip_whitespace(const char *input, size_t offset, size_t max_len) {
  while (offset < max_len && (input[offset] == ' ' || input[offset] == '\t')) {
    offset++;
  }
  return offset;
}

// Parse HTTP method (GET, POST, etc.)
Result<std::pair<Http::Method, size_t> >
Http::Request::Parser::parse_method(const char *input, size_t offset) {
  std::string method_str;
  size_t start = offset;
  
  // Read until space
  while (input[offset] != '\0' && input[offset] != ' ' && input[offset] != '\r') {
    method_str += input[offset];
    offset++;
  }
  
  if (method_str.empty()) {
    return ERR_PAIR(Http::Method, size_t, Errors::invalid_format);
  }
  
  // Convert to uppercase for comparison
  std::transform(method_str.begin(), method_str.end(), method_str.begin(), to_upper);
  
  Http::Method method;
  if (method_str == "GET")
    method = Http::GET;
  else if (method_str == "HEAD")
    method = Http::HEAD;
  else if (method_str == "OPTIONS")
    method = Http::OPTIONS;
  else if (method_str == "POST")
    method = Http::POST;
  else if (method_str == "DELETE")
    method = Http::DELETE;
  else if (method_str == "PUT")
    method = Http::PUT;
  else if (method_str == "CONNECT")
    method = Http::CONNECT;
  else if (method_str == "TRACE")
    method = Http::TRACE;
  else if (method_str == "PATCH")
    method = Http::PATCH;
  else
    return ERR_PAIR(Http::Method, size_t, Errors::invalid_format);
  
  return OK_PAIR(Http::Method, size_t, method, offset - start);
}

// Parse request path
Result<std::pair<std::string, size_t> >
Http::Request::Parser::parse_path(const char *input, size_t offset) {
  std::string path;
  size_t start = offset;
  
  // Read until space
  while (input[offset] != '\0' && input[offset] != ' ' && input[offset] != '\r') {
    path += input[offset];
    offset++;
  }
  
  if (path.empty()) {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  return OK_PAIR(std::string, size_t, path, offset - start);
}

// Parse HTTP version (HTTP/1.1)
Result<std::pair<std::string, size_t> >
Http::Request::Parser::parse_http_version(const char *input, size_t offset) {
  std::string version;
  size_t start = offset;
  
  // Read until \r\n
  while (input[offset] != '\0' && input[offset] != '\r') {
    version += input[offset];
    offset++;
  }
  
  if (version.empty()) {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  // Verify it's HTTP/1.1 or similar
  if (version.find("HTTP/") != 0) {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  return OK_PAIR(std::string, size_t, version, offset - start);
}

// Parse request line (e.g., "GET /path HTTP/1.1\r\n")
Result<std::pair<Http::Request *, size_t> >
Http::Request::Parser::parse_request_line(const char *input, size_t offset) {
  size_t start_offset = offset;
  
  // Parse method
  Result<std::pair<Http::Method, size_t> > method_res = parse_method(input, offset);
  if (!method_res.error().empty()) {
    return ERR_PAIR(Http::Request *, size_t, method_res.error());
  }
  
  Http::Method method = method_res.value()->first;
  offset += method_res.value()->second;
  
  // Skip space
  offset = skip_whitespace(input, offset, offset + 100);
  
  // Parse path
  Result<std::pair<std::string, size_t> > path_res = parse_path(input, offset);
  if (!path_res.error().empty()) {
    return ERR_PAIR(Http::Request *, size_t, path_res.error());
  }
  
  std::string path = path_res.value()->first;
  offset += path_res.value()->second;
  
  // Skip space
  offset = skip_whitespace(input, offset, offset + 100);
  
  // Parse HTTP version
  Result<std::pair<std::string, size_t> > version_res = parse_http_version(input, offset);
  if (!version_res.error().empty()) {
    return ERR_PAIR(Http::Request *, size_t, version_res.error());
  }
  
  offset += version_res.value()->second;
  
  // Skip \r\n
  if (input[offset] == '\r' && input[offset + 1] == '\n') {
    offset += 2;
  } else {
    return ERR_PAIR(Http::Request *, size_t, Errors::invalid_format);
  }
  
  // For now, create a simple request with empty body
  Http::Body::Value empty_val;
  empty_val._null = NULL;
  Http::Body body(Http::Body::Empty, empty_val);
  
  Http::Request *req = new Http::Request(method, path, body);
  
  return OK_PAIR(Http::Request *, size_t, req, offset - start_offset);
}

// Parse headers (e.g., "Host: example.com\r\n")
Result<std::pair<std::map<std::string, Json>, size_t> >
Http::Request::Parser::parse_headers(const char *input, size_t offset) {
  typedef std::map<std::string, Json> HeaderMap;
  HeaderMap headers;
  size_t start_offset = offset;
  
  // Parse headers until we hit empty line (\r\n\r\n)
  while (true) {
    // Check for end of headers (empty line)
    if (input[offset] == '\r' && input[offset + 1] == '\n') {
      offset += 2;
      break;
    }
    
    // Parse header name
    std::string header_name;
    while (input[offset] != '\0' && input[offset] != ':') {
      header_name += input[offset];
      offset++;
    }
    
    if (input[offset] != ':') {
      return ERR_PAIR(HeaderMap, size_t, Errors::invalid_format);
    }
    offset++; // skip ':'
    
    // Skip whitespace after colon
    offset = skip_whitespace(input, offset, offset + 100);
    
    // Parse header value
    std::string header_value;
    while (input[offset] != '\0' && input[offset] != '\r') {
      header_value += input[offset];
      offset++;
    }
    
    // Skip \r\n
    if (input[offset] == '\r' && input[offset + 1] == '\n') {
      offset += 2;
    } else {
      return ERR_PAIR(HeaderMap, size_t, Errors::invalid_format);
    }
    
    // Store header as Json string
    headers[header_name] = *Json::str(header_value);
  }
  
  return OK_PAIR(HeaderMap, size_t, headers, offset - start_offset);
}

// Parse body
Result<std::pair<Http::Body *, size_t> >
Http::Request::Parser::parse_body(const char *, size_t, 
                                   std::map<std::string, Json> const &) {
  // For now, just return empty body
  // A full implementation would parse the body based on Content-Type and Content-Length
  Http::Body::Value empty_val;
  empty_val._null = NULL;
  return OK_PAIR(Http::Body *, size_t, new Http::Body(Http::Body::Empty, empty_val), 0);
}

// Main parse function
Result<std::pair<Http::Request *, size_t> >
Http::Request::Parser::parse(const char *input, size_t) {
  if (input == NULL) {
    return ERR_PAIR(Http::Request *, size_t, Errors::invalid_format);
  }
  
  size_t offset = 0;
  
  // Parse request line
  Result<std::pair<Http::Request *, size_t> > req_line_res = 
      parse_request_line(input, offset);
  if (!req_line_res.error().empty()) {
    return req_line_res;
  }
  
  Http::Request *request = req_line_res.value()->first;
  offset += req_line_res.value()->second;
  
  // Parse headers
  Result<std::pair<std::map<std::string, Json>, size_t> > headers_res = 
      parse_headers(input, offset);
  if (!headers_res.error().empty()) {
    delete request;
    return ERR_PAIR(Http::Request *, size_t, headers_res.error());
  }
  
  // Update request headers
  request->_headers = headers_res.value()->first;
  offset += headers_res.value()->second;
  
  // Parse body
  Result<std::pair<Http::Body *, size_t> > body_res = 
      parse_body(input, offset, request->_headers);
  if (!body_res.error().empty()) {
    delete request;
    return ERR_PAIR(Http::Request *, size_t, body_res.error());
  }
  
  // Update request body
  request->_body = *body_res.value()->first;
  offset += body_res.value()->second;
  
  return OK_PAIR(Http::Request *, size_t, request, offset);
}

// Original parse function (delegating to Parser::parse)
Result<std::pair<Http::Request *, size_t> >
Http::Request::parse(const char *input, char delimiter) {
  // Find the length of the input until delimiter
  size_t len = 0;
  while (input[len] != delimiter && input[len] != '\0') {
    len++;
  }
  
  return Http::Request::Parser::parse(input, len);
}
