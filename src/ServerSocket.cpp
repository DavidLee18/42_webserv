#include "ServerSocket.hpp"
#include "epoll_kqueue.h"
#include "file_descriptor.h"

Result<FileDescriptor> init_server()
{
	Result<FileDescriptor> socket_result = FileDescriptor::socket_new();
	if (!socket_result.has_value())
		return socket_result;
	FileDescriptor socket_fd = socket_result.value_mut();

	int option = 1;
	Result<Void> opt_result = socket_fd.set_socket_option();

	int epoll = epoll_create(10);

	socket_fd.set_nonblocking();
}