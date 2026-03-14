#ifndef UWSGI_SERVER_H
#define UWSGI_SERVER_H

#include <map>
#include <string>
#include <vector>

/**
 * @class UwsgiServer
 * @brief A server that speaks the uwsgi binary protocol.
 *
 * Listens on a TCP port, accepts connections from a front-end proxy (e.g.
 * nginx configured with uwsgi_pass), parses the uwsgi packet, executes the
 * configured Python WSGI script as a child process, and forwards the HTTP
 * response back to the caller.
 *
 * uwsgi packet layout (all integers are little-endian):
 *   [modifier1 : 1 byte ] – 0 = Python/WSGI
 *   [datasize  : 2 bytes] – byte length of the vars block that follows
 *   [modifier2 : 1 byte ] – 0 = default sub-protocol
 *   [vars block: datasize bytes] – sequence of key/value pairs:
 *       [key_len : 2 bytes][key : key_len bytes]
 *       [val_len : 2 bytes][val : val_len bytes]
 *   [body      : CONTENT_LENGTH bytes, if present in vars]
 */
class UwsgiServer {
public:
  UwsgiServer(const std::string &script_path, int port);
  ~UwsgiServer();

  void run();

private:
  std::string _script_path;
  int _port;
  int _server_fd;

  bool setup_socket();
  void handle_connection(int client_fd);
  bool read_all(int fd, void *buf, size_t len);
  bool parse_uwsgi_vars(const std::vector<unsigned char> &data,
                        std::map<std::string, std::string> &vars);
  std::string execute_wsgi(const std::map<std::string, std::string> &vars,
                           const std::string &body);
  void send_error_response(int fd, int status, const std::string &reason);

  // Non-copyable
  UwsgiServer(const UwsgiServer &);
  UwsgiServer &operator=(const UwsgiServer &);
};

#endif
