#include "ServerSocket.hpp"
#include "epoll_kqueue.h"
#include "file_descriptor.h"

void init_server()
{
	FileDescriptor server;

	server = FileDescriptor::socket_new();
}