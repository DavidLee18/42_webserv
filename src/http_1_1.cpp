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
// RFC 2616 §4.2: Headers can be continued on next line with leading space/tab
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
    
    // Parse header value (may span multiple lines)
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
    
    // RFC 2616 §4.2: Check for header continuation (line folding)
    // Continuation lines start with space or tab
    while (input[offset] == ' ' || input[offset] == '\t') {
      // This is a continuation line
      // Skip the leading whitespace
      while (input[offset] != '\0' && (input[offset] == ' ' || input[offset] == '\t')) {
        offset++;
      }
      
      // Add a space to separate from previous line content
      header_value += ' ';
      
      // Parse the continuation line value
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

// Helper function for URL decoding (application/x-www-form-urlencoded)
static std::string url_decode(const std::string &encoded) {
  std::string decoded;
  size_t i = 0;
  
  while (i < encoded.length()) {
    if (encoded[i] == '%' && i + 2 < encoded.length()) {
      // Decode hex character (ensure we have at least 2 chars after %)
      char hex[3] = {encoded[i + 1], encoded[i + 2], '\0'};
      char *end;
      long value = std::strtol(hex, &end, 16);
      if (end == hex + 2) {
        decoded += static_cast<char>(value);
        i += 3;
        continue;
      }
    } else if (encoded[i] == '+') {
      // Plus signs represent spaces in form encoding
      decoded += ' ';
      i++;
      continue;
    }
    decoded += encoded[i];
    i++;
  }
  
  return decoded;
}

// Parse application/x-www-form-urlencoded body
static Result<std::pair<std::map<std::string, std::string> *, size_t> >
parse_form_urlencoded(const char *input, size_t offset, size_t body_length) {
  std::map<std::string, std::string> *form = new std::map<std::string, std::string>();
  std::string body_str(input + offset, body_length);
  
  size_t pos = 0;
  while (pos < body_str.length()) {
    // Find the next '&' or end of string
    size_t amp_pos = body_str.find('&', pos);
    if (amp_pos == std::string::npos) {
      amp_pos = body_str.length();
    }
    
    std::string pair = body_str.substr(pos, amp_pos - pos);
    
    // Split on '='
    size_t eq_pos = pair.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = url_decode(pair.substr(0, eq_pos));
      std::string value = url_decode(pair.substr(eq_pos + 1));
      (*form)[key] = value;
    } else {
      // Key without value
      (*form)[url_decode(pair)] = "";
    }
    
    pos = amp_pos + 1;
  }
  
  typedef std::map<std::string, std::string> FormMap;
  return OK_PAIR(FormMap *, size_t, form, body_length);
}

// Parse body
// RFC 2616 §4.3: Message body determined by Content-Type and Content-Length
Result<std::pair<Http::Body *, size_t> >
Http::Request::Parser::parse_body(const char *input, size_t offset, 
                                   std::map<std::string, Json> const &headers) {
  // Check for Content-Length header
  std::map<std::string, Json>::const_iterator content_length_it = 
      headers.find("content-length");
  
  // If no Content-Length, return empty body
  if (content_length_it == headers.end()) {
    Http::Body::Value empty_val;
    empty_val._null = NULL;
    return OK_PAIR(Http::Body *, size_t, new Http::Body(Http::Body::Empty, empty_val), 0);
  }
  
  // Parse Content-Length value
  if (content_length_it->second.type() != Json::Str) {
    Http::Body::Value empty_val;
    empty_val._null = NULL;
    return OK_PAIR(Http::Body *, size_t, new Http::Body(Http::Body::Empty, empty_val), 0);
  }
  
  std::string length_str = *content_length_it->second.value()._str;
  char *end_ptr = NULL;
  unsigned long body_length = std::strtoul(length_str.c_str(), &end_ptr, 10);
  
  if (end_ptr == length_str.c_str() || *end_ptr != '\0') {
    // Invalid Content-Length
    Http::Body::Value empty_val;
    empty_val._null = NULL;
    return OK_PAIR(Http::Body *, size_t, new Http::Body(Http::Body::Empty, empty_val), 0);
  }
  
  // RFC 2616: Content-Length of 0 is valid and indicates empty body
  if (body_length == 0) {
    Http::Body::Value empty_val;
    empty_val._null = NULL;
    return OK_PAIR(Http::Body *, size_t, new Http::Body(Http::Body::Empty, empty_val), 0);
  }
  
  // Check Content-Type header to determine body type
  std::map<std::string, Json>::const_iterator content_type_it = 
      headers.find("content-type");
  
  std::string content_type;
  if (content_type_it != headers.end() && content_type_it->second.type() == Json::Str) {
    content_type = *content_type_it->second.value()._str;
    // Normalize to lowercase for comparison
    for (size_t i = 0; i < content_type.length(); i++) {
      content_type[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(content_type[i])));
    }
  }
  
  Http::Body::Value body_val;
  Http::Body::Type body_type;
  
  // Determine body type based on Content-Type
  if (content_type.find("application/json") != std::string::npos) {
    // Parse as JSON
    // Create a temporary null-terminated string for JSON parser
    std::string json_body(input + offset, body_length);
    Result<std::pair<Json *, size_t> > json_res = 
        Json::Parser::parse(json_body.c_str(), '\0');
    
    if (!json_res.error().empty()) {
      // JSON parsing failed, treat as raw HTML
      body_type = Http::Body::Html;
      body_val.html_raw = new std::string(input + offset, body_length);
    } else {
      body_type = Http::Body::HttpJson;
      body_val.json = json_res.value()->first;
    }
  } else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
    // Parse as form-urlencoded
    Result<std::pair<std::map<std::string, std::string> *, size_t> > form_res = 
        parse_form_urlencoded(input, offset, static_cast<size_t>(body_length));
    
    if (!form_res.error().empty()) {
      // Form parsing failed, treat as raw HTML
      body_type = Http::Body::Html;
      body_val.html_raw = new std::string(input + offset, body_length);
    } else {
      body_type = Http::Body::HttpFormUrlEncoded;
      body_val.form = form_res.value()->first;
    }
  } else {
    // Default: treat as raw HTML/text
    body_type = Http::Body::Html;
    body_val.html_raw = new std::string(input + offset, body_length);
  }
  
  return OK_PAIR(Http::Body *, size_t, new Http::Body(body_type, body_val), 
                 static_cast<size_t>(body_length));
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
