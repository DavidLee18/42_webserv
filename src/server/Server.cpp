#include "../webserv.h"
#include "Server.hpp"

void Server::new_connection(const FileDescriptor *server_fd)
{
	while (true)
	{ // Edge-Triggered이므로 모든 연결을 다 받아야 함
		Result<FileDescriptor> client_result = server_fd->socket_accept(NULL, NULL);
		if (!client_result.has_value())
		{
			const std::string &err = client_result.error();
			if (err == Errors::try_again)
				break; // EWOULDBLOCK: 더 이상 대기 중인 연결 없음
			else if (err == Errors::interrupted)
				continue; // EINTR: accept 재시도
			else
			{
				std::cerr << "ERROR: accept failed: " << err << std::endl;
				break;
			}
		}

		FileDescriptor client_fd = client_result.value();
		if (!client_fd.set_nonblocking().has_value())
		{
			std::cerr << "ERROR: failed to set client socket to non-blocking mode" << std::endl;
			continue;
		}

		// 클라이언트 소켓을 EPoll에 등록 (읽기/쓰기 감지, ET 모드)
		Event client_event(&client_fd, true, true, false, false, false, false);
		Option client_option(true, false, false, false);

		Result<FileDescriptor *> add_result = epoll.add_fd(client_fd, client_event, client_option);
		if (add_result.has_value())
		{
			FileDescriptor *client_ptr = add_result.value();
			ClientSession session;
			// ★ 내 문지기(server_fd)의 Config 설정을 그대로 복사해서 세션에 넣음!
			if (listeners.find(server_fd) != listeners.end())
			{
				session.config = listeners.at(server_fd);
			}
			clients[client_ptr] = session;
			std::cout << "New client connected!" << std::endl;
		}
		else
		{
			std::cerr << "ERROR: epoll add failed: " << add_result.error() << std::endl;
		}
	}
}

void Server::disconnect(const FileDescriptor *client_fd)
{
	std::cout << "Client disconnected" << std::endl;
	epoll.del_fd(*client_fd);
	clients.erase(client_fd);
}

void Server::client_read(const FileDescriptor *client_fd)
{
	while (true)
	{ // ET 모드이므로 버퍼가 빌 때까지 다 읽음
		char buf[NETWORK_BUFFER_SIZE];
		Result<ssize_t> recv_res = client_fd->sock_recv(buf, sizeof(buf));
		if (!recv_res.has_value())
			break; // EWOULDBLOCK

		ssize_t bytes = recv_res.value();
		if (bytes == 0)
		{
			// 클라이언트가 정상적으로 연결 종료 (EOF)
			disconnect(client_fd);
			return;
		}
		clients.at(client_fd).in_buff.append(buf, static_cast<std::size_t>(bytes));
	}

	// HTTP 파싱 및 응답 생성 로직
	if (clients.find(client_fd) != clients.end() && !clients.at(client_fd).in_buff.empty())
	{
		std::string &in_buffer = clients.at(client_fd).in_buff;
		size_t header_end = in_buffer.find("\r\n\r\n");

		if (header_end != std::string::npos)
		{
			Result<std::pair<Http::Request *, size_t>> request_result = Http::Request::parse(in_buffer.c_str(), '\0');
			if (!request_result.has_value())
			{
				std::cerr << request_result.error() << std::endl;
				return;
			}

			Http::Request *request = request_result.value().first;
			std::cout << "[Request] " << request->method() << " " << request->path() << std::endl;

			HttpResponse http = Response::generate(request, clients.at(client_fd).config);

			// HTTP 응답 메시지 조립
			// todo: 하드코딩된 response 말고 동적으로
			std::ostringstream server_response;
			server_response << "HTTP/1.1 " << http.status_code << "\r\n";
			server_response << "Content-Type: text/html\r\n";
			server_response << "Content-Length: " << http.body.length() << "\r\n";
			server_response << "Connection: keep-alive\r\n\r\n";
			server_response << http.body;

			clients.at(client_fd).out_buff += server_response.str();
			in_buffer.erase(0, header_end + 4);
		}
	}
}

void Server::client_write(const FileDescriptor *client_fd)
{
	if (clients.find(client_fd) == clients.end())
		return;

	std::string &write_buffer = clients.at(client_fd).out_buff;
	if (!write_buffer.empty())
	{
		while (true)
		{ // ET 모드이므로 보낼 수 있는 만큼 다 보냄
			Result<ssize_t> send_res = client_fd->sock_send(write_buffer.c_str(), write_buffer.length());
			if (!send_res.has_value())
				break; // EWOULDBLOCK

			ssize_t bytes = send_res.value();
			if (bytes == 0)
				break;

			write_buffer.erase(0, static_cast<std::size_t>(bytes));
			if (write_buffer.empty())
				break;
		}
	}
}

Result<Void> Server::init()
{
	// EPoll init
	Result<EPoll> epoll_result = EPoll::create(1024);
	if (!epoll_result.has_value())
		return ERR(Void, epoll_result.error());
	epoll = epoll_result.value();

	// Init server socket for every port listed on configuration file
	const std::map<unsigned int, ServerConfig> &servers = config.Get_ServerConfig_map();
	for (std::map<unsigned int, ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it)
	{
		unsigned short port = static_cast<unsigned short>(it->first);

		// Init socket
		Result<FileDescriptor> sock_result = FileDescriptor::socket_new();
		if (!sock_result.has_value())
			return ERR(Void, sock_result.error());
		FileDescriptor server_fd = sock_result.value();

		// Non-blocking socket for ET (edge-triggered)
		Result<Void> nb_result = server_fd.set_nonblocking();
		if (!nb_result.has_value())
			return ERR(Void, nb_result.error());

		// Port reusing option
		int opt = 1;
		Result<Void> reuseaddr_result = server_fd.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (!reuseaddr_result.has_value())
			std::cerr << "WARNING: SO_REUSEADDR failed: " << reuseaddr_result.error() << std::endl;

		// Bind (associate IP and port)
		struct in_addr addr;
		addr.s_addr = htonl(INADDR_ANY); // All IPs
		Result<Void> bind_result = server_fd.socket_bind(addr, port);
		if (!bind_result.has_value())
			return ERR(Void, bind_result.error());

		// Listen (max queue length)
		Result<Void> listen_result = server_fd.socket_listen(SOMAXCONN);
		if (!listen_result.has_value())
			return ERR(Void, listen_result.error());

		// EPoll event and option setting
		Event event(&server_fd, true, false, false, false, false, false); // in=true
		Option op(true, false, false, false);							  // et=true

		// Add server socket to EPoll
		Result<FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
		if (!add_result.has_value())
			return ERR(Void, add_result.error());

		// Save pointer to distinguish server sockets from client sockets
		FileDescriptor *fd_ptr = add_result.value();
		listeners[fd_ptr] = &it->second;

		std::cout << "Server listening on port " << port << std::endl;
	}

	return OK(Void, Void());
}

Result<Void> Server::start()
{
	while (true)
	{
		// Waiting for events using epoll
		Result<Events> events_result = epoll.wait(-1);
		if (!events_result.has_value())
		{
			if (events_result.error() == Errors::interrupted)
				continue;
			else
			{
				std::cerr << "ERROR: " << events_result.error() << std::endl;
				break;
			}
		}

		Events events = events_result.value();
		while (!events.is_end())
		{
			Result<const Event *> ev_result = *events;
			if (!ev_result.has_value())
			{
				++events;
				continue;
			}

			const Event *event = ev_result.value();
			const FileDescriptor *fd = event->fd;

			// 1. 서버 소켓(문지기)인 경우 (listeners map에 Key가 존재함)
			if (listeners.find(fd) != listeners.end())
			{
				new_connection(fd);
			}
			// 2. 이미 연결된 클라이언트 소켓인 경우
			else
			{
				// 에러 감지 시 끊기
				if (event->err || event->hup || event->rdhup)
				{
					disconnect(fd);
				}
				else
				{
					// 읽기 권한이 생겼을 때 읽기
					if (event->in)
						client_read(fd);

					// 쓰기 권한이 생겼을 때 쓰기
					if (event->out)
						client_write(fd);
				}
			}
			++events;
		}
	}

	clients.clear();
	return OK(Void, Void());
}
