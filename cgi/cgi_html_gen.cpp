// CGI/1.1-conforming program that generates a dynamic HTML page.
// It reads CGI meta-variables from the environment, obtains the current
// server time, and writes a complete HTTP response (headers + body) to
// stdout so the web server can forward it to the client.
//
// Compile:  c++ -Wall -Wextra -Werror -std=c++98 -o cgi_html_gen cgi_html_gen.cpp
// Usage:    executed by the webserver as a CGI script

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

static std::string env_or_empty(const char *name) {
  const char *val = std::getenv(name);
  return val ? std::string(val) : std::string();
}

static std::string html_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
    case '&':  out += "&amp;";  break;
    case '<':  out += "&lt;";   break;
    case '>':  out += "&gt;";   break;
    case '"':  out += "&quot;"; break;
    case '\'': out += "&#39;";  break;
    default:   out += s[i];     break;
    }
  }
  return out;
}

int main() {
  // --- gather CGI meta-variables ---
  const std::string request_method  = env_or_empty("REQUEST_METHOD");
  const std::string script_name     = env_or_empty("SCRIPT_NAME");
  const std::string path_info       = env_or_empty("PATH_INFO");
  const std::string query_string    = env_or_empty("QUERY_STRING");
  const std::string remote_addr     = env_or_empty("REMOTE_ADDR");
  const std::string server_name     = env_or_empty("SERVER_NAME");
  const std::string server_port     = env_or_empty("SERVER_PORT");
  const std::string server_protocol = env_or_empty("SERVER_PROTOCOL");
  const std::string server_software = env_or_empty("SERVER_SOFTWARE");
  const std::string gateway_iface   = env_or_empty("GATEWAY_INTERFACE");
  const std::string content_type    = env_or_empty("CONTENT_TYPE");
  const std::string content_length  = env_or_empty("CONTENT_LENGTH");
  const std::string http_host       = env_or_empty("HTTP_HOST");
  const std::string http_user_agent = env_or_empty("HTTP_USER_AGENT");
  const std::string http_accept     = env_or_empty("HTTP_ACCEPT");

  // --- get current UTC time ---
  char time_buf[64];
  std::time_t now = std::time(NULL);
  std::tm *utc = std::gmtime(&now);
  if (utc) {
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", utc);
  } else {
    time_buf[0] = '\0';
  }

  // --- build HTML body ---
  std::string body;
  body += "<!DOCTYPE html>\n";
  body += "<html lang=\"en\">\n";
  body += "<head>\n";
  body += "  <meta charset=\"UTF-8\">\n";
  body += "  <title>CGI Dynamic Page</title>\n";
  body += "  <style>\n";
  body += "    body { font-family: monospace; margin: 2em; }\n";
  body += "    table { border-collapse: collapse; }\n";
  body += "    th, td { border: 1px solid #ccc; padding: 4px 8px; text-align: left; }\n";
  body += "    th { background: #eee; }\n";
  body += "  </style>\n";
  body += "</head>\n";
  body += "<body>\n";
  body += "  <h1>CGI/1.1 Dynamic Response</h1>\n";
  body += "  <p>Generated at: <strong>" + std::string(time_buf) + "</strong></p>\n";
  body += "  <h2>CGI Meta-Variables</h2>\n";
  body += "  <table>\n";
  body += "    <tr><th>Variable</th><th>Value</th></tr>\n";

  struct CgiVariable { const char *name; const std::string &value; };
  CgiVariable vars[] = {
    { "GATEWAY_INTERFACE", gateway_iface   },
    { "REQUEST_METHOD",    request_method  },
    { "SCRIPT_NAME",       script_name     },
    { "PATH_INFO",         path_info       },
    { "QUERY_STRING",      query_string    },
    { "REMOTE_ADDR",       remote_addr     },
    { "SERVER_NAME",       server_name     },
    { "SERVER_PORT",       server_port     },
    { "SERVER_PROTOCOL",   server_protocol },
    { "SERVER_SOFTWARE",   server_software },
    { "CONTENT_TYPE",      content_type    },
    { "CONTENT_LENGTH",    content_length  },
    { "HTTP_HOST",         http_host       },
    { "HTTP_USER_AGENT",   http_user_agent },
    { "HTTP_ACCEPT",       http_accept     },
  };
  const std::size_t nvars = sizeof(vars) / sizeof(vars[0]);

  for (std::size_t i = 0; i < nvars; ++i) {
    body += "    <tr><td>" + std::string(vars[i].name) + "</td><td>" +
            html_escape(vars[i].value) + "</td></tr>\n";
  }

  body += "  </table>\n";
  body += "</body>\n";
  body += "</html>\n";

  // --- emit CGI response headers, then the body ---
  // Per RFC 3875 §6.2.1 a CGI script MUST send at least a Content-Type header.
  std::cout << "Content-Type: text/html; charset=UTF-8\r\n";
  std::cout << "Status: 200 OK\r\n";
  std::cout << "\r\n";
  std::cout << body;

  return 0;
}
