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
    #ifdef __APPLE__
    #else
    EPoll e_poll;
    #endif
    signal(SIGINT, wrap_up);
    while (sig == 0) {
        std::cout << "Hello, World!" << std::endl;
    }
    return 0;
}

void wrap_up(const int signum) { sig = signum; }