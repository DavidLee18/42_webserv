#include "ServerSocket.hpp"

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds)
{
	// EPoll init
	Result<EPoll> epoll_result = EPoll::create(1024);
	if (!epoll_result.has_value())
		return epoll_result;
	EPoll epoll = epoll_result.value();

	// Init server socket for every port listed on configuration file
	const std::map<unsigned int, ServerConfig> &servers = config.Get_ServerConfig_map();
	for (std::map<unsigned int, ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it)
	{
		unsigned short port = static_cast<unsigned short>(it->first);

		// Init socket
		Result<FileDescriptor> sock_result = FileDescriptor::socket_new();
		if (!sock_result.has_value())
			return ERR(EPoll, sock_result.error());
		FileDescriptor server_fd = sock_result.value();

		// Non-blocking socket for ET(edge triggered)
		Result<Void> nb_result = server_fd.set_nonblocking();
		if (!nb_result.has_value())
			return ERR(EPoll, nb_result.error());

		// Port reusing option
		int opt = 1;
		server_fd.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		// Bind (IP-Port connect)
		struct in_addr addr;
		addr.s_addr = htonl(INADDR_ANY); // All IP
		Result<Void> bind_result = server_fd.socket_bind(addr, port);
		if (!bind_result.has_value())
			return ERR(EPoll, bind_result.error());

		// Listen (max queue length)
		Result<Void> listen_result = server_fd.socket_listen(SOMAXCONN);
		if (!listen_result.has_value())
			return ERR(EPoll, listen_result.error());

		// EPoll event and option setting
		Event ev(&server_fd, true, false, false, false, false, false); // in=true
		Option op(true, false, false, false);						   // et=true

		// Add server socket to EPoll
		Result<const FileDescriptor *> add_result = epoll.add_fd(server_fd, ev, op);
		if (!add_result.has_value())
			return ERR(EPoll, add_result.error());

		// Saving to separate Server socket and Client sockets 
		server_fds.insert(add_result.value());
		std::cout << "Server listening on port " << port << std::endl;
	}

	return OK(EPoll, epoll);
}

void run_server(EPoll &epoll, const std::set<const FileDescriptor *> &server_fds)
{
	std::map<const FileDescriptor *, ClientConnection> clients;

	while (true)
	{
		// 이벤트 대기 (-1은 무한 대기)
		Result<Events> events_res = epoll.wait(-1);
		if (!events_res.has_value())
			continue;

		Events events = events_res.value();
		while (!events.is_end())
		{
			Result<const Event *> ev_res = *events;
			if (!ev_res.has_value())
			{
				++events;
				continue;
			}
			const Event *ev = ev_res.value();
			const FileDescriptor *fd = ev->fd;
			FileDescriptor *mut_fd = const_cast<FileDescriptor *>(fd); // accept, recv 등을 위해 const 해제

			// 1. 서버 소켓에 이벤트가 발생한 경우 (새로운 클라이언트 접속)
			if (server_fds.find(fd) != server_fds.end())
			{
				while (true)
				{ // Edge-Triggered이므로 가능한 모든 연결을 accept 해야 함
					Result<FileDescriptor> client_res = mut_fd->socket_accept(NULL, NULL);
					if (!client_res.has_value())
					{
						break; // EWOULDBLOCK: 더 이상 대기 중인 연결이 없음
					}
					FileDescriptor client_fd = client_res.value();
					client_fd.set_nonblocking(); // 클라이언트 소켓도 논블로킹 필수!

					// 클라이언트 소켓을 EPoll에 등록 (읽기/쓰기 감지, Edge-Triggered)
					Event client_ev(&client_fd, true, true, false, false, false, false); // in=true, out=true
					Option client_op(true, false, false, false);						 // et=true

					Result<const FileDescriptor *> add_result = epoll.add_fd(client_fd, client_ev, client_op);
					if (add_result.has_value())
					{
						const FileDescriptor *new_client_ptr = add_result.value();
						clients.insert(std::make_pair(
											new_client_ptr,
											ClientConnection(new_client_ptr)));
						std::cout << "New client connected!" << std::endl;
					}
				}
			}
			// 2. 클라이언트 소켓에 이벤트가 발생한 경우 (데이터 송수신)
			else
			{
				// 에러나 연결 종료 감지
				if (ev->err || ev->hup || ev->rdhup)
				{
					std::cout << "Client disconnected (error/hup)" << std::endl;
					epoll.del_fd(*mut_fd);
					clients.erase(fd);
					++events;
					continue;
				}

				// 읽기 이벤트 (클라이언트가 데이터를 보냄)
				if (ev->in)
				{
					while (true)
					{ // Edge-Triggered이므로 모든 데이터를 읽어야 함
						char buf[4096];
						Result<ssize_t> recv_res = mut_fd->sock_recv(buf, sizeof(buf));
						if (!recv_res.has_value())
						{
							break; // EWOULDBLOCK: 더 이상 읽을 데이터 없음
						}
						ssize_t bytes = recv_res.value();
						if (bytes == 0)
						{ // 클라이언트가 정상적으로 연결 종료 (EOF)
							std::cout << "Client disconnected (EOF)" << std::endl;
							epoll.del_fd(*mut_fd);
							clients.erase(fd);
							break;
						}
						// 읽은 데이터를 버퍼에 저장
						clients.at(fd).read_buffer.append(buf, static_cast<std::size_t>(bytes));
					}

					// TODO: 여기서 HTTP 파싱 로직 호출
					// 임시로, 데이터가 들어오면 무조건 고정된 응답을 보내도록 설정
					if (clients.find(fd) != clients.end() && !clients.at(fd).read_buffer.empty())
					{
						std::string body = "<html><body><h1>Hello from webserv!</h1></body></html>";
						std::ostringstream response;
						response << "HTTP/1.1 200 OK\r\n";
						response << "Content-Type: text/html\r\n";
						response << "Content-Length: " << body.length() << "\r\n";
						response << "Connection: keep-alive\r\n\r\n";
						response << body;

						clients.at(fd).write_buffer = response.str();
						clients.at(fd).read_buffer.clear(); // 읽은 데이터 비우기
					}
				}

				// 쓰기 이벤트 (클라이언트에게 데이터를 보낼 수 있음)
				if (ev->out && clients.find(fd) != clients.end())
				{
					ClientConnection &client = clients.at(fd);
					if (!client.write_buffer.empty())
					{
						while (true)
						{ // Edge-Triggered이므로 보낼 수 있는 만큼 다 보내야 함
							Result<ssize_t> send_res = mut_fd->sock_send(client.write_buffer.c_str(), client.write_buffer.length());
							if (!send_res.has_value())
							{
								break; // EWOULDBLOCK: 소켓 버퍼가 꽉 차서 더 못 보냄
							}
							ssize_t bytes = send_res.value();
							client.write_buffer.erase(0, static_cast<std::size_t>(bytes)); // 보낸 만큼 버퍼에서 삭제
							if (client.write_buffer.empty())
							{
								break; // 다 보냈음
							}
						}
					}
				}
			}
			++events;
		}
	}
}
