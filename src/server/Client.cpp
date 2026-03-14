#include "Client.hpp"

Request::Method Request::get_method() const
{
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
}

const std::string Request::get_connection_string() const
{
  if (keep_alive)
    return "keep-alive";
  else
    return "close";
}

Request::Request(std::string request)
{
  std::stringstream ss(request);
  std::string line;

  // 1. 첫 번째 줄(Request Line)만 읽기
  if (std::getline(ss, line)) {
    // CRLF 대응 (\r 제거)
    if (!line.empty() && line[line.size() - 1] == '\r')
      line.erase(line.size() - 1);

    std::stringstream line_ss(line);

    line_ss >> this->method;  // "GET"
    line_ss >> this->path;  // "/index.html"
    line_ss >> this->version; // "HTTP/1.1"
  }
}
