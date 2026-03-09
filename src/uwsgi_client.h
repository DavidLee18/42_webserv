#ifndef UWSGI_CLIENT_H
#define UWSGI_CLIENT_H

#include "result.h"
#include <map>
#include <string>

// UwsgiClient speaks the uWSGI binary protocol over a TCP connection.
// It encodes a WSGI/CGI vars block and an optional request body, sends them to
// a running uwsgi_server, and returns the raw HTTP response output produced by
// the WSGI script.
class UwsgiClient {
  std::string _host;
  int _port;

  UwsgiClient(const UwsgiClient &);
  UwsgiClient &operator=(const UwsgiClient &);

public:
  UwsgiClient(const std::string &host, int port);

  // Encode vars + body as a uwsgi binary request, send to host:port,
  // and return the raw HTTP response (as produced by the WSGI script).
  Result<std::string> send(const std::map<std::string, std::string> &vars,
                           const std::string &body) const;
};

#endif // UWSGI_CLIENT_H
