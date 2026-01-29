#include <iostream>
#include <sys/epoll.h>
#include <errno.h>
#include <cstring>

int main() {
    int fd = epoll_create(1000);
    if (fd < 0) {
        std::cerr << "epoll_create failed: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
        return 1;
    }
    std::cout << "epoll_create succeeded, fd=" << fd << std::endl;
    return 0;
}
