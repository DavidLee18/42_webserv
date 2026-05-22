#include "Response.hpp"
#include "DefaultError.hpp"
#include "ResponseUtils.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

static std::ostream &identity_stream(std::ostream &os) { return os; }

static void write_response_head(std::ostream &os, const Response &resp,
                                std::ostream &(*prefix)(std::ostream &)) {
  os << prefix << "HTTP/1.1 "
     << DefaultError::status_code_to_string(resp.status_code) << utils::crlf;
  os << prefix << "Date: " << ResponseUtils::get_http_date() << utils::crlf;
  os << prefix << "Server: webserv" << utils::crlf;
  if ((resp.status_code == Response::MOVED_PERMANENTLY ||
       resp.status_code == Response::FOUND) &&
      !resp.redir.empty())
    os << prefix << "Location: " << resp.redir << utils::crlf;
  os << prefix << "Content-Type: " << resp.content_type << utils::crlf;
  if (!resp.cookie.empty())
    os << prefix << "Set-Cookie: " << resp.cookie << utils::crlf;
  for (std::map<std::string, std::string>::const_iterator it =
           resp.headers.begin();
       it != resp.headers.end(); ++it)
    os << prefix << it->first << ": " << it->second << utils::crlf;
  os << prefix << "Content-Length: " << resp.content_length << utils::crlf;
  if (resp.keep_alive)
    os << prefix << "Connection: keep-alive" << utils::crlf;
  else
    os << prefix << "Connection: close" << utils::crlf;
  os << utils::crlf;
}

std::ostream &operator<<(std::ostream &os, Response const &resp) {
  write_response_head(os, resp, identity_stream);
  if (!resp.body.empty())
    os << resp.body;
  return os;
}

void Response::print_simple(std::ostream &os) const {
  write_response_head(os, *this, utils::info);
}

Result<Response>
Response::from_cgi_outbuff(std::string const&                        cgi_out,
                           std::map<std::string, std::string> const& headers) {
    std::cerr << "[CGI-PARSE] raw output (" << cgi_out.length() << " bytes):\n"
              << cgi_out << "\n[CGI-PARSE-END]\n";
    size_t bound_pos = cgi_out.find("\r\n\r\n");
    size_t bound_len = 4;

    if (bound_pos == std::string::npos) {
        bound_pos = cgi_out.find("\n\n");
        bound_len = 2;
    }
    if (bound_pos == std::string::npos)
        return ERR(Response, Errors::bad_gateway);

    std::string header_part(cgi_out.substr(0, bound_pos));
    Response    resp;

    resp.headers = headers;
    resp.body    = cgi_out.substr(bound_pos + bound_len);

    std::istringstream iss(header_part);
    std::string        header_line;
    while (std::getline(iss, header_line) && !iss.fail()) {
        size_t colon_pos = header_line.find(':');
        if (colon_pos == std::string::npos) continue;
        std::string header_name  = header_line.substr(0, colon_pos);
        std::string header_value = header_line.substr(colon_pos + 1);
        if (!header_value.empty() &&
            header_value[header_value.length() - 1] == '\r')
            header_value = header_value.substr(0, header_value.length() - 1);
        size_t whitespace_pos = header_value.find_first_not_of(" \t");
        if (whitespace_pos == std::string::npos)
            return ERR(Response, Errors::bad_gateway);
        header_value   = header_value.substr(whitespace_pos);
        whitespace_pos = header_value.find_last_not_of(" \t");
        if (whitespace_pos != std::string::npos)
            header_value = header_value.substr(0, whitespace_pos + 1);
        if (!utils::is_header_name(header_name) ||
            !utils::is_header_value(header_value))
            return ERR(Response, Errors::bad_gateway);
        std::string header_name_lower(header_name.size(), '\0');
        std::transform(header_name.begin(), header_name.end(),
                       header_name_lower.begin(), utils::tolower);
        for (std::map<std::string, std::string>::iterator it =
                 resp.headers.begin();
             it != resp.headers.end(); ++it) {
            std::string header_name_lower_(it->first.size(), '\0');
            std::transform(it->first.begin(), it->first.end(),
                           header_name_lower_.begin(), utils::tolower);
            if (header_name_lower_ == header_name_lower) {
                std::cerr << "[CGI-PARSE-DUP] incoming='" << header_name
                          << "' collides with seeded='" << it->first << "'"
                          << std::endl;
                return ERR(Response, Errors::bad_gateway);
            }
        }
        resp.headers[header_name] = header_value;
    }

    bool status_found = false, content_length_found = false,
         content_type_found = false;
    for (std::map<std::string, std::string>::iterator it = resp.headers.begin();
         it != resp.headers.end();) {
        std::string header_name_lower(it->first.size(), '\0');
        std::transform(it->first.begin(), it->first.end(),
                       header_name_lower.begin(), utils::tolower);

        if (header_name_lower == "content-length") {
            std::istringstream iss_(it->second);
            iss_ >> resp.content_length;
            if (iss_.fail()) return ERR(Response, Errors::bad_gateway);
            resp.headers.erase(it++);
            content_length_found = true;
            continue;
        }

        if (header_name_lower == "status") {
            std::istringstream iss_(it->second);
            unsigned short     status_code;
            iss_ >> status_code;
            if (iss_.fail() || status_code < 100 || status_code > 599)
                return ERR(Response, Errors::bad_gateway);
            resp.status_code = DefaultError::int_to_status_code(status_code);
            resp.headers.erase(it++);
            status_found = true;
            continue;
        }

        if (header_name_lower == "content-type") {
            resp.content_type = it->second;
            resp.headers.erase(it++);
            content_type_found = true;
            continue;
        }

        if (header_name_lower == "set-cookie") {
            resp.cookie = it->second;
            resp.headers.erase(it++);
            continue;
        }

        if (header_name_lower == "connection") {
            resp.keep_alive = it->second == "keep-alive";
            resp.headers.erase(it++);
            continue;
        }

        ++it;
    }

    if (!status_found) resp.status_code = Response::OK;
    if (!content_length_found) resp.content_length = resp.body.length();
    if (!content_type_found) resp.content_type = "text/html";
    if (resp.status_code == Response::MOVED_PERMANENTLY ||
        resp.status_code == Response::FOUND) {
        if (resp.headers.find("location") != resp.headers.end())
            resp.redir = get_string_from_map(resp.headers, "location");
        else if (resp.headers.find("Location") != resp.headers.end())
            resp.redir = get_string_from_map(resp.headers, "Location");
        else
            return ERR(Response, Errors::bad_gateway);
        resp.headers.erase("location");
        resp.headers.erase("Location");
    }

    return OK(Response, resp);
}
