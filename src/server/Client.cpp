#include "Client.hpp"
#include "../errors.h"

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

const std::string Request::get_connection_string() const {
  if (keep_alive)
    return "keep-alive";
  else
    return "close";
}

std::string Request::get_cookie_value(const std::string &name) const {
  if (cookie.empty())
    return "";

  std::string target = name + "=";
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
    return ERR(Request *,
               Errors::incomplete_header); // 헤더가 다 안 들어왔으면 다음 epoll
                                           // 이벤트 대기

  // checking Content-Length
  std::string header_lower = buff.substr(0, header_end);
  for (size_t i = 0; i < header_lower.length(); ++i) {
    header_lower[i] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(header_lower[i])));
  }
  const size_t cl_pos = header_lower.find("content-length:");
  const bool has_te =
      (header_lower.find("transfer-encoding:") != std::string::npos);
  if (cl_pos == std::string::npos)
    return ERR(Request *, Errors::malformed_header);
  if (has_te)
    return ERR(Request *, Errors::malformed_header); // conforming to the RFC
  const char *str =
      header_lower.c_str() + cl_pos + std::strlen("content-length:");
  while (*str == ' ' || *str == '\t')
    str++;
  if (*str == '-')
    return ERR(Request *, Errors::malformed_header);
  char *end;
  const size_t content_length = std::strtoul(str, &end, 10);
  while (*end == ' ' || *end == '\t')
    ++end;
  if (*end != '\r' && *end != '\n')
    return ERR(Request *, Errors::malformed_header);
  if (header_lower.find("content-length:", cl_pos + 1) != std::string::npos)
    return ERR(Request *, Errors::malformed_header);

  std::stringstream ss(buff);
  std::string line;

  Request::Method method;
  std::string req_path;
  std::string req_version;
  // 1. 첫 번째 줄(Request Line)만 읽기
  if (std::getline(ss, line)) {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    std::stringstream line_ss(line);
    std::string method_str;

    line_ss >> method_str;  // "GET"
    line_ss >> req_path;    // "/"
    line_ss >> req_version; // "HTTP/1.1"
    if (req_version != "HTTP/1.0" && req_version != "HTTP/1.1")
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

    if (cl_pos == std::string::npos && !has_te &&
        (method == POST || method == PUT || method == PATCH))
      return ERR(Request *, Errors::malformed_header);
  } else {
    return ERR(Request *, Errors::internal_server_error);
  }
  Request *req = new Request(method, req_path, req_version, content_length);

  while (std::getline(ss, line) && line != "\r" && line != "") {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    const size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);

      // Value 앞쪽에 있는 공백 지워주기 (예: ": localhost" -> "localhost")
      const size_t first_non_space = value.find_first_not_of(" \t");
      if (first_non_space != std::string::npos) {
        value = value.substr(first_non_space);
      } else {
        value = "";
      }
      req->header[key] = value;
    }
  }

  if (get_string_from_map(req->header, "Connection") == "keep-alive")
    req->keep_alive = true;

  req->cookie = get_string_from_map(req->header, "Cookie");

  // check body if body not full break to get more event
  const size_t total_request_len =
      header_end + std::strlen("\r\n\r\n") + req->content_length;
  if (buff.length() < total_request_len) {
    req->remnants = buff.substr(header_end + std::strlen("\r\n\r\n"));
    buff.erase(0, total_request_len);
    return OK(Request *, req);
  }

  const std::streampos pos = ss.tellg();
  if (pos != std::streampos(-1))
    req->body = buff.substr(static_cast<size_t>(pos));

  if (req->body.empty() || req->body.size() == req->content_length)
    req->remnants = "";
  buff.erase(0, total_request_len);
  return OK(Request *, req);
}

void Request::continue_parsing(std::string &buff) {
  if (remnants.empty())
    return;
  else if (remnants.length() < content_length) {
    if (remnants.length() + buff.length() >= content_length) {
      size_t diff = content_length - remnants.length();
      remnants += buff.substr(0, diff);
      buff.erase(0, diff);
      body = remnants;
      remnants.clear();
    } else {
      remnants += buff;
      buff.clear();
    }
  }
}
