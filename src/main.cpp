#include "webserv.h"

volatile sig_atomic_t g_receivedSignal = 0;

int main(const int argc, char *argv[]) {
  (void)argc;
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1]);
  if (!fd.error().empty()) {
    std::cerr << "file open failed: " << fd.error() << std::endl;
    return 1;
  }
  Result<WebserverConfig> result_config = WebserverConfig::parse(fd.value_mut());
  if (!result_config.error().empty()) {
    std::cerr << "config parsing failed: " << result_config.error()
              << std::endl;
    return 1;
  } else {
    const WebserverConfig &config = result_config.value();
    // Initiate server.
    Server server(config);
    Result<Void> init_result = server.init();
    if (!init_result.has_value()) {
      std::cerr << "Server init failed: " << init_result.error() << std::endl;
      return 1;
    }

    std::cout << "Starting server loop..." << std::endl;
    server.start();
  }
  return 0;
}

void wrap_up(const int signum) throw() { g_receivedSignal = signum; }
