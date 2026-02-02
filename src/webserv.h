#ifndef WEBSERV_H
#define WEBSERV_H

#define BUFFER_SIZE 42
#define LONG_DOUBLE_DIGITS 37
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

#include "ParsingUtils.hpp"
#include "ServerConfig.hpp"
#include "WebserverConfig.hpp"
#include "cgi_1_1.h"
#include "epoll_kqueue.h"
#include "errors.h"
#include "http_1_1.h"
#include "json.h"
#include "wsgi.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void wrap_up(int) throw();

#endif
