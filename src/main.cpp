//
// Created by 이재현 on 2025-11-08.
//
#include "webserv.h"

int main(const int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: webserv <config_file>" << std::endl;
        return 1;
    }
    (void)argv;
    signal(SIGINT, wrap_up);
    #ifdef __APPLE__
    KQueue k_queue(1000);
    #else
    EPoll e_poll(1000);
    #endif
    while (sig == 0) {
        std::cout << "Hello, World!" << std::endl;
    }
    return 0;
}

void wrap_up(const int signum) throw() { sig = signum; }