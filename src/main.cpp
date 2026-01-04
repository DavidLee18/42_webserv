#include "webserv.h"

volatile sig_atomic_t sig = 0;

int main(const int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: webserv <config_file>" << std::endl;
    return 1;
  }
  Result<MapRecord<Json *, size_t> > res = Json::Parser::parse(argv[1], '\0');
  PANIC(res)
  Json *js = res.value()->key;
  std::cout << *js << std::endl;
  delete js;
  return 0;
  // std::string config_path(argv[1]);
  // signal(SIGINT, wrap_up);
#ifdef __APPLE__
  KQueue k_queue(1000);
#else
  // EPoll *ep;
  // Result<EPoll> rep = EPoll::create(1000);
  // if (rep.err != NULL) {
  //   std::cerr << rep.err << std::endl;
  //   return 1;
  // }
  // ep = rep.val;
  // // add the events
  // Result<Events> w;
  // while (sig == 0) {
  //   std::cout << "Hello, World!" << std::endl;
  //   w = ep->wait(100);
  //   PANIC(w)
  //   for (Events evs = *w.val; !evs.is_end(); ++evs) {
  //     Result<const Event *> re = *evs;
  //     PANIC(re)
  //     const Event **ev = re.val;
  //     // process events
  //   }
  // }
#endif
  return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }
