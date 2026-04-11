#include "Client.hpp"

Request::Method Request::get_method() const {
  if (method == "GET")
    return Request::GET;
  else if (method == "HEAD")
    return Request::HEAD;
  else if (method == "OPTIONS")
    return Request::OPTIONS;
  else if (method == "POST")
    return Request::POST;
  else if (method == "DELETE")
    return Request::DELETE;
  else if (method == "PUT")
    return Request::PUT;
  else if (method == "CONNECT")
    return Request::CONNECT;
  else if (method == "TRACE")
    return Request::TRACE;
  else if (method == "PATCH")
    return Request::PATCH;
  return Request::ERROR;
}

const std::string Request::get_connection_string() const {
  if (keep_alive)
    return "keep-alive";
  else
    return "close";
}

Request::Request(std::string request) {
  std::stringstream ss(request);
  std::string line;

  // 1. 첫 번째 줄(Request Line)만 읽기
  if (std::getline(ss, line)) {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    std::stringstream line_ss(line);

    line_ss >> this->method;  // "GET"
    line_ss >> this->path;    // "/"
    line_ss >> this->version; // "HTTP/1.1"
  }
  while (std::getline(ss, line) && line != "\r" && line != "") {
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);

      // Value 앞쪽에 있는 공백 지워주기 (예: ": localhost" -> "localhost")
      size_t first_non_space = value.find_first_not_of(" \t");
      if (first_non_space != std::string::npos) {
        value = value.substr(first_non_space);
      } else {
        value = "";
      }
      this->header[key] = value;
    }
  }
  std::streampos pos = ss.tellg();
  if (pos != std::streampos(-1)) {
    this->body = request.substr(static_cast<size_t>(pos));
  }

  if (get_string_from_map(header, "Connection") == "keep-alive")
    keep_alive = true;
  else
    keep_alive = false;

  cookie = get_string_from_map(header, "Cookie");

  content_full = true;
  if (!body.empty())
  {
    int content_len = atoi(get_string_from_map(header, "Content-Length").c_str());
    if (body.size() != static_cast<size_t>(content_len))
      content_full = false;
  }
}
