#include "uwsgi_server.h"

#include <cstdlib>
#include <iostream>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: uwsgi_server <script.py> [port]" << std::endl;
    return 1;
  }

  const std::string script_path = argv[1];

  // Verify the script file exists before starting the server
  struct stat st;
  if (stat(script_path.c_str(), &st) != 0) {
    std::cerr << "Script not found: " << script_path << std::endl;
    return 1;
  }

  int port = 9000; // default uWSGI port
  if (argc == 3) {
    port = std::atoi(argv[2]);
    if (port <= 0 || port > 65535) {
      std::cerr << "Invalid port number: " << argv[2] << std::endl;
      return 1;
    }
  }

  UwsgiServer server(script_path, port);
  server.run();

  return 0;
}
