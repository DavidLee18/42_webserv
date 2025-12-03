//
// Created by 이재현 on 2025-11-08.
//

#ifndef WEBSERV_H
#define WEBSERV_H

#include "epoll_kqueue.h"
#include "errors.h"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void wrap_up(int) throw();

#endif
