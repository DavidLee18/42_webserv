//
// Created by 이재현 on 2025-11-08.
//

#ifndef WEBSERV_H
#define WEBSERV_H
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "exceptions.h"
#include "epoll_kqueue.h"

volatile sig_atomic_t sig = 0;

void wrap_up(int) throw();

#endif