#include "webserv.h"

// Helper function to normalize header field names to lowercase
// RFC 2616 §4.2: Field names are case-insensitive
static std::string normalize_header_name(const std::string &name) {
  std::string normalized = name;
  for (size_t i = 0; i < normalized.length(); i++) {
    normalized[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(normalized[i])));
  }
  return normalized;
}

// Helper function to trim whitespace from both ends of a string
// RFC 2616 §4.2: LWS before/after field-content may be removed
static std::string trim_whitespace(const std::string &str) {
  size_t start = 0;
  size_t end = str.length();
  
  // Trim leading whitespace
  while (start < end && (str[start] == ' ' || str[start] == '\t')) {
    start++;
  }
  
  // Trim trailing whitespace
  while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t')) {
    end--;
  }
  
  return str.substr(start, end - start);
}

// Helper function to skip whitespace
static size_t skip_whitespace(const char *input, size_t offset, size_t max_len) {
  while (offset < max_len && (input[offset] == ' ' || input[offset] == '\t')) {
    offset++;
  }
  return offset;
}

// Parse HTTP method (GET, POST, etc.)
// RFC 2616 §5.1.1: Method is case-sensitive
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
  
  // RFC 2616 §5.1.1: Methods are case-sensitive, no conversion
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
// RFC 2616 §3.1: HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
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
  
  // RFC 2616 §3.1: Verify format HTTP/digit.digit
  if (version.find("HTTP/") != 0 || version.length() < 8) {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  // Validate version format: HTTP/x.y where x and y are digits
  size_t slash_pos = 5; // Position after "HTTP/"
  if (slash_pos >= version.length() || !std::isdigit(version[slash_pos])) {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  // Skip major version digits
  size_t dot_pos = slash_pos;
  while (dot_pos < version.length() && std::isdigit(version[dot_pos])) {
    dot_pos++;
  }
  
  // Check for dot separator
  if (dot_pos >= version.length() || version[dot_pos] != '.') {
    return ERR_PAIR(std::string, size_t, Errors::invalid_format);
  }
  
  // Check minor version has at least one digit
  dot_pos++;
  if (dot_pos >= version.length() || !std::isdigit(version[dot_pos])) {
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
  
  // Skip space (limited search)
  size_t space_limit = offset + 10; // reasonable limit for spaces
  offset = skip_whitespace(input, offset, space_limit);
  
  // Parse path
  Result<std::pair<std::string, size_t> > path_res = parse_path(input, offset);
  if (!path_res.error().empty()) {
    return ERR_PAIR(Http::Request *, size_t, path_res.error());
  }
  
  std::string path = path_res.value()->first;
  offset += path_res.value()->second;
  
  // Skip space (limited search)
  space_limit = offset + 10; // reasonable limit for spaces
  offset = skip_whitespace(input, offset, space_limit);
  
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
// RFC 2616 §4.2: Field names are case-insensitive, LWS may be present
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
    
    // Skip whitespace after colon (limited search)
    size_t ws_limit = offset + 10; // reasonable limit for whitespace
    offset = skip_whitespace(input, offset, ws_limit);
    
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
    
    // RFC 2616 §4.2: Normalize header name to lowercase (case-insensitive)
    std::string normalized_name = normalize_header_name(header_name);
    
    // RFC 2616 §4.2: Trim leading/trailing whitespace from value
    std::string trimmed_value = trim_whitespace(header_value);
    
    // Store header as Json string
    headers[normalized_name] = *Json::str(trimmed_value);
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
