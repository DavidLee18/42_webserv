// CGI/1.1-conforming program that generates a dynamic HTML page.
// It reads CGI meta-variables from the environment, obtains the current
// server time, and writes a CGI response (headers + body) to stdout;
// the web server then constructs the HTTP response that is sent to the client.
//
// Compile:  use the project's Makefile (target `cgi`) to build this program.
// Usage:    executed by the webserver as a CGI script

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <string>

// environ is declared in <unistd.h> on POSIX systems.
#include <unistd.h>

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

static std::string tr(const std::string &name, const std::string &value) {
  return "    <tr><td>" + html_escape(name) + "</td><td>" +
         html_escape(value) + "</td></tr>\n";
}

int main() {
  // --- get current UTC time ---
  char time_buf[64];
  std::time_t now = std::time(NULL);
  std::tm utc_tm;
  if (gmtime_r(&now, &utc_tm)) {
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
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

  // --- section 1: all 17 standard CGI/1.1 meta-variables (RFC 3875 §4.1) ---
  body += "  <h2>Standard CGI/1.1 Meta-Variables</h2>\n";
  body += "  <table>\n";
  body += "    <tr><th>Variable</th><th>Value</th></tr>\n";

  // The fixed set defined by RFC 3875 §4.1
  static const char *const STANDARD_VARS[] = {
    "AUTH_TYPE",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "GATEWAY_INTERFACE",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "QUERY_STRING",
    "REMOTE_ADDR",
    "REMOTE_HOST",
    "REMOTE_IDENT",
    "REMOTE_USER",
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE",
    NULL
  };

  for (int i = 0; STANDARD_VARS[i] != NULL; ++i) {
    body += tr(STANDARD_VARS[i], env_or_empty(STANDARD_VARS[i]));
  }

  body += "  </table>\n";

  // --- section 2: HTTP_* request-header variables ---
  // Iterate the process environment to capture every HTTP_* variable the
  // server forwarded, regardless of which headers the client sent.
  std::map<std::string, std::string> http_vars;
  for (char **ep = environ; ep && *ep; ++ep) {
    const char *entry = *ep;
    if (std::strncmp(entry, "HTTP_", 5) == 0) {
      const char *eq = std::strchr(entry, '=');
      if (eq) {
        http_vars[std::string(entry, static_cast<std::size_t>(eq - entry))] =
            std::string(eq + 1);
      }
    }
  }

  body += "  <h2>HTTP Request-Header Variables (HTTP_*)</h2>\n";
  body += "  <table>\n";
  body += "    <tr><th>Variable</th><th>Value</th></tr>\n";
  if (http_vars.empty()) {
    body += "    <tr><td colspan=\"2\">(none)</td></tr>\n";
  } else {
    for (std::map<std::string, std::string>::const_iterator it =
             http_vars.begin(); it != http_vars.end(); ++it) {
      body += tr(it->first, it->second);
    }
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
