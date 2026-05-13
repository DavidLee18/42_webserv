#include "webserv.h"

volatile sig_atomic_t g_receivedSignal = 0;

int main(const int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, wrap_up);
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1]);
  PANIC(fd)
  const Result<WebserverConfig> result_config =
      WebserverConfig::parse(fd.value_mut());
  PANIC(result_config)
  // std::cout << result_config.value() << std::endl;
  // const WebserverConfig &config = result_config.value();
  // Server server(config);
  // const Result<Void> init_result = server.init();
  // PANIC(init_result)
  // const Result<Void> server_result = server.start();
  // PANIC(server_result)
  return 0;
}

void wrap_up(const int signum) throw() { g_receivedSignal = signum; }
