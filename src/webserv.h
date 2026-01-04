//
// Created by 이재현 on 2025-11-08.
//

#ifndef WEBSERV_H
#define WEBSERV_H

#define BUFFER_SIZE 42
#define LONG_DOUBLE_DIGITS 37

#include "epoll_kqueue.h"
#include "errors.h"
#include "http_1_1.h"
#include "json.h"
#include "map_record.h"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void wrap_up(int) throw();

#endif
