#include "Client.hpp"
#include "../errors.h"

ClientSession::~ClientSession() {
  if (req != NULL)
    delete req;
}

std::string Request::get_method_string() const {
  if (method == GET)
    return "GET";
  else if (method == HEAD)
    return "HEAD";
  else if (method == OPTIONS)
    return "OPTIONS";
  else if (method == POST)
    return "POST";
  else if (method == DELETE)
    return "DELETE";
  else if (method == PUT)
    return "PUT";
  else if (method == CONNECT)
    return "CONNECT";
  else if (method == TRACE)
    return "TRACE";
  else if (method == PATCH)
    return "PATCH";
  return "";
}

std::string Request::get_connection_string() const {
  if (keep_alive)
    return "keep-alive";
  else
    return "close";
}

std::string Request::get_cookie_value(const std::string &name) const {
  if (cookie.empty())
    return "";

  const std::string target = name + "=";
  size_t start = cookie.find(target);
  if (start == std::string::npos)
    return "";

  start += target.length();
  size_t end = cookie.find(';', start);
  if (end == std::string::npos)
    end = cookie.length();

  return cookie.substr(start, end - start);
}

Result<Request *> Request::from_buff(std::string &buff) {
  const size_t header_end = buff.find("\r\n\r\n");
  if (header_end == std::string::npos)
    return ERR(Request *, Errors::incomplete_header);
  size_t content_length = 0;

  std::string header_lower = buff.substr(0, header_end);
  for (size_t i = 0; i < header_lower.length(); ++i) {
    header_lower[i] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(header_lower[i])));
  }
  const size_t host_pos = header_lower.find("host:");
  // Check if there's only ONE host header (not counting it as a substring)
  // We look for it as a header name, which must be preceded by \r\n or be at
  // start
  size_t second_host_pos = std::string::npos;
  size_t search_from = host_pos + 5; // Skip the found "host:" itself
  while ((second_host_pos = header_lower.find("host:", search_from)) !=
         std::string::npos) {
    // Check if this "host:" is at the beginning of a line (preceded by \r\n)
    if (second_host_pos >= 2 && header_lower[second_host_pos - 2] == '\r' &&
        header_lower[second_host_pos - 1] == '\n') {
      break; // Found a second host header
    }
    search_from = second_host_pos + 5;
  }

  if (host_pos == std::string::npos || second_host_pos != std::string::npos)
    return ERR(Request *, Errors::bad_request);
  const size_t cl_pos = header_lower.find("content-length:");
  const size_t te_pos = header_lower.find("transfer-encoding:");
  bool decode_chunked = false;
  if (cl_pos != std::string::npos && te_pos != std::string::npos)
    return ERR(Request *, Errors::malformed_header); // conforming to the RFC
  if (te_pos != std::string::npos &&
      (header_lower.find("transfer-encoding:chunked") != std::string::npos ||
       header_lower.find("transfer-encoding: chunked") != std::string::npos))
    decode_chunked = true;
  if (cl_pos != std::string::npos && !decode_chunked) {
    const char *str =
        header_lower.c_str() + cl_pos + std::strlen("content-length:");
    while (*str == ' ' || *str == '\t')
      str++;
    if (*str == '-')
      return ERR(Request *, Errors::malformed_header);
    char *end;
    content_length = std::strtoul(str, &end, 10);
    while (*end == ' ' || *end == '\t')
      ++end;
    if (*end != '\r' && *end != '\n')
      return ERR(Request *, Errors::malformed_header);
    if (header_lower.find("content-length:", cl_pos + 1) != std::string::npos)
      return ERR(Request *, Errors::malformed_header);
  }

  std::stringstream ss(buff);
  std::string line;

  Request::Method method;
  std::string req_path;
  std::string req_version;
  // 1. 첫 번째 줄(Request Line)만 읽기
  if (std::getline(ss, line) && !ss.fail()) {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    std::stringstream line_ss(line);
    std::string method_str;

    line_ss >> method_str;  // "GET"
    line_ss >> req_path;    // "/"
    line_ss >> req_version; // "HTTP/1.1"
    if (req_version != "HTTP/1.0" && req_version != "HTTP/1.1")
      return ERR(Request *, Errors::bad_request);
    if (line.length() > 0x2000 || req_path.find('\0') != std::string::npos)
      return ERR(Request *, Errors::bad_request);
    if (method_str == "GET")
      method = GET;
    else if (method_str == "HEAD")
      method = HEAD;
    else if (method_str == "OPTIONS")
      method = OPTIONS;
    else if (method_str == "POST")
      method = POST;
    else if (method_str == "DELETE")
      method = DELETE;
    else if (method_str == "PUT")
      method = PUT;
    else if (method_str == "CONNECT")
      method = CONNECT;
    else if (method_str == "TRACE")
      method = TRACE;
    else if (method_str == "PATCH")
      method = PATCH;
    else
      return ERR(Request *, Errors::bad_request);

    if (cl_pos == std::string::npos && te_pos == std::string::npos &&
        (method == POST || method == PUT || method == PATCH))
      return ERR(Request *, Errors::malformed_header);
  } else {
    return ERR(Request *, Errors::internal_server_error);
  }
  Request *req;
  if (!decode_chunked)
    req = new Request(method, req_path, req_version, content_length);
  else
    req = new Request(method, req_path, req_version);

  while (std::getline(ss, line) && line != "\r" && !line.empty()) {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    const size_t colon_pos = line.find(':');
    if (line.length() > 0x2000 || line[0] == ' ' || line[0] == '\t' ||
        colon_pos == std::string::npos || colon_pos == 0 ||
        line[colon_pos - 1] == ' ' || line[colon_pos - 1] == '\t') {
      delete req;
      return ERR(Request *, Errors::bad_request);
    }
    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    // Value 앞쪽에 있는 공백 지워주기 (예: ": localhost" -> "localhost")
    const size_t first_non_space = value.find_first_not_of(" \t");
    if (first_non_space != std::string::npos) {
      value = value.substr(first_non_space);
    } else {
      value = "";
    }
    for (std::string::const_iterator it = value.begin(); it != value.end();
         ++it) {
      if (*it < 0x20 && *it != '\t') {
        delete req;
        return ERR(Request *, Errors::bad_request);
      }
    }
    req->header[key] = value;
    if (req->header.size() > 100) {
      delete req;
      return ERR(Request *, Errors::bad_request);
    }
  }

  std::string connection_header =
      get_string_from_map(req->header, "Connection");
  // If empty (no Connection header), HTTP/1.1 defaults to keep-alive
  if (connection_header.empty())
    req->header["Connection"] = "keep-alive";
  // keep_alive defaults to true; only set false if client explicitly requests
  // close
  if (connection_header == "close")
    req->keep_alive = false;

  req->cookie = get_string_from_map(req->header, "Cookie");
  if (req->decode_chunk_state == NOT_CHUNKED) {
    // check body; if body is not full, break to get more event
    const size_t total_request_len = header_end + std::strlen("\r\n\r\n") +
                                     static_cast<size_t>(req->content_length);
    if (buff.length() < total_request_len) {
      req->remnants = buff.substr(header_end + std::strlen("\r\n\r\n"));
      buff.erase(0, total_request_len);
      return OK(Request *, req);
    }

    const std::streampos pos = ss.tellg();
    if (pos == std::streampos(-1)) {
      delete req;
      return ERR(Request *, "streampos error");
    }
    req->body = buff.substr(static_cast<size_t>(pos));

    if (req->body.empty() ||
        req->body.size() == static_cast<size_t>(req->content_length))
      req->remnants.clear();
    buff.erase(0, total_request_len);
    return OK(Request *, req);
  } else {
    const std::streampos body_start = ss.tellg();
    if (body_start == std::streampos(-1)) {
      delete req;
      return ERR(Request *, "streampos error");
    }
    req->remnants = buff.substr(static_cast<size_t>(body_start));
    size_t chunk_end;

    if (req->remnants.compare(0, 3, "0\r\n") == 0) {
      chunk_end = 0;
    } else {
      const size_t crlf_zero = req->remnants.find("\r\n0\r\n");
      chunk_end =
          (crlf_zero == std::string::npos) ? std::string::npos : crlf_zero + 2;
    }
    if (chunk_end != std::string::npos) {
      const Result<size_t> unchunked = req->unchunk(chunk_end);
      if (!unchunked.has_value()) {
        delete req;
        return ERR(Request *, Errors::bad_request);
      }
      buff.erase(0, static_cast<size_t>(body_start) + unchunked.value());
    } // else: chunked but not yet complete — leave buff untouched; remnants
      // holds the partial
    return OK(Request *, req);
  }
}

Result<Void> Request::continue_parsing(std::string &buff) {
  if (remnants.empty())
    return OKV;
  else if (decode_chunk_state == NOT_CHUNKED &&
           remnants.length() < static_cast<size_t>(content_length)) {
    if (remnants.length() + buff.length() >=
        static_cast<size_t>(content_length)) {
      const size_t diff =
          static_cast<size_t>(content_length) - remnants.length();
      remnants += buff.substr(0, diff);
      buff.erase(0, diff);
      body = remnants;
      remnants.clear();
      return OKV;
    } else {
      remnants += buff;
      buff.clear();
      return OKV;
    }
  } else if (decode_chunk_state != NOT_CHUNKED && decode_chunk_state != DONE) {
    remnants += buff;
    buff.clear();
    size_t chunk_end;

    if (remnants.compare(0, 3, "0\r\n") == 0) {
      chunk_end = 0;
    } else {
      const size_t crlf_zero = remnants.find("\r\n0\r\n");
      chunk_end =
          (crlf_zero == std::string::npos) ? std::string::npos : crlf_zero + 2;
    }
    if (chunk_end != std::string::npos) {
      const Result<size_t> unchunked = unchunk(chunk_end);
      if (!unchunked.has_value())
        return ERR(Void, Errors::bad_request);
    }
    return OKV;
  }
  return ERR(Void, Errors::bad_request);
}

bool Request::is_partial() const {
  if (decode_chunk_state == NOT_CHUNKED)
    return !remnants.empty();
  return decode_chunk_state != DONE;
}

Result<size_t> Request::unchunk(size_t remnant_end) {
  if (remnant_end >= remnants.length())
    return ERR(size_t, Errors::bad_request);
  const size_t last_pos =
      remnants.find("\r\n", remnant_end + std::strlen("0\r\n"));
  if (last_pos == std::string::npos)
    return ERR(size_t, Errors::bad_request);
  std::string rems(remnants.substr(0, remnant_end + std::strlen("0\r\n")));
  size_t line_end = 0;
  if (rems.empty())
    return ERR(size_t, Errors::bad_request);
  rems.append("\r\n");
  std::string size_or_data;
  size_t chunk_size = 0;
  size_t size_line_semicolon_pos = 0;
  while (!rems.empty()) {
    line_end = rems.find("\r\n");
    if (line_end == std::string::npos)
      return ERR(size_t, Errors::bad_request);
    size_or_data = rems.substr(0, line_end);
    rems = rems.substr(line_end + std::strlen("\r\n"));
    size_line_semicolon_pos = size_or_data.find(';');
    if (size_line_semicolon_pos != std::string::npos)
      size_or_data = size_or_data.substr(0, size_line_semicolon_pos);
    for (std::string::const_iterator it = size_or_data.begin();
         it != size_or_data.end(); ++it) {
      chunk_size *= 16;
      if (*it >= '0' && *it <= '9')
        chunk_size += static_cast<size_t>(*it - '0');
      else if (*it >= 'A' && *it <= 'F')
        chunk_size += static_cast<size_t>(*it - 'A') + 10;
      else if (*it >= 'a' && *it <= 'f')
        chunk_size += static_cast<size_t>(*it - 'a') + 10;
      else
        return ERR(size_t, Errors::bad_request);
    }
    if (chunk_size == 0) {
      if (rems.substr(0, 2) != "\r\n")
        return ERR(size_t, Errors::bad_request);
      rems = rems.substr(std::strlen("\r\n"));
      if (!rems.empty())
        return ERR(size_t, Errors::bad_request);
      else {
        content_length = static_cast<ssize_t>(body.length());
        decode_chunk_state = DONE;
        remnants.clear();
        return OK(size_t, last_pos + 2);
      }
    } else if (chunk_size > 10 * 1024 * 1024 || chunk_size + 2 > rems.length())
      return ERR(
          size_t,
          Errors::bad_request); // Safe only because caller ensures full body is
                                // buffered (find("0\r\n\r\n") succeeded).
    body += rems.substr(0, chunk_size);
    if (rems.substr(chunk_size, 2) != "\r\n")
      return ERR(size_t, Errors::bad_request);
    rems = rems.substr(chunk_size + std::strlen("\r\n"));
    chunk_size = 0;
  }
  // \r\n0\r\n not found yet remnant string empty
  return ERR(size_t, Errors::bad_request);
}

Result<size_t> Request::get_content_length() const {
  if (content_length < 0)
    return ERR(size_t, Errors::access_denied);
  else
    return OK(size_t, static_cast<size_t>(content_length));
}

bool Request::is_chunked() const {
  return decode_chunk_state != NOT_CHUNKED && decode_chunk_state != DONE;
}
