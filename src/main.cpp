#include "webserv.h"

volatile sig_atomic_t sig = 0;

int main(const int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<std::pair<Json, size_t> > res = Json::Parser::parse(argv[2], '\0');
  PANIC(res)
  Json js = res.value().first;
  std::cout << js << std::endl;
  WebserverConfig config(argv[1]);
  return 0;
  signal(SIGINT, wrap_up);
#ifdef __APPLE__
  KQueue k_queue(1000);
#else
  Result<EPoll> rep = EPoll::create(1000);
  PANIC(rep)
  EPoll ep = rep.value();
  while (sig == 0) {
    std::cout << "Hello, World!" << std::endl;
    Result<Events> w = ep.wait(100);
    PANIC(w)
    for (Events evs = w.value(); !evs.is_end(); ++evs) {
      Result<const Event *> re = *evs;
      PANIC(re)
      const Event *const ev = re.value();
      // process events
    }
  }
#endif
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
