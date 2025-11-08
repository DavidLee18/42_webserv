//
// Created by 이재현 on 2025-11-08.
//

#ifndef WEBSERV_H
#define WEBSERV_H
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <csignal>
#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#else
#include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif //WEBSERV_H