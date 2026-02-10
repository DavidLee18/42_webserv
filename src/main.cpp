#include "webserv.h"

volatile sig_atomic_t sig = 0;

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
  Result<WebserverConfig> result_config =
      WebserverConfig::parse(fd.value_mut());
  if (!result_config.error().empty()) {
    std::cerr << "config parsing failed: " << result_config.error()
              << std::endl;
    return 1;
  } else {
    const WebserverConfig &config = result_config.value();
    std::cout << "main(): " << std::endl;
    ServerConfig const &sconf = config.Get_ServerConfig_map().at(80);
    RouteRule const &rule =
        sconf.Get_Routes().at(std::make_pair(Http::GET, "old_stuff"));
    std::cout << rule.path << std::endl;
  }
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
