#include "ServerSocket.hpp"

#define MAX_FDS 1024

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds)
{
	// EPoll 생성
	Result<EPoll> epoll_res = EPoll::create(MAX_FDS);
	if (!epoll_res.has_value())
		return epoll_res;
	EPoll epoll = epoll_res.value();

	// 2. 설정 파일에 있는 모든 포트에 대해 서버 소켓 생성
	const std::map<unsigned int, ServerConfig> &servers = config.Get_ServerConfig_map();
	for (std::map<unsigned int, ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it)
	{
		unsigned int port = it->first;

		// 소켓 생성
		Result<FileDescriptor> sock_res = FileDescriptor::socket_new();
		if (!sock_res.has_value())
			return ERR(EPoll, sock_res.error());
		FileDescriptor server_fd = sock_res.value();

		// 논블로킹 설정 (Edge-Triggered 사용을 위해 필수)
		Result<Void> nb_res = server_fd.set_nonblocking();
		if (!nb_res.has_value())
			return ERR(EPoll, nb_res.error());

		// 포트 재사용 옵션 설정 (서버 재시작 시 "Address already in use" 에러 방지)
		int opt = 1;
		server_fd.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		// Bind (IP와 포트 연결)
		struct in_addr addr;
		addr.s_addr = htonl(INADDR_ANY); // 모든 IP 허용
		Result<Void> bind_res = server_fd.socket_bind(addr, static_cast<unsigned short>(port));
		if (!bind_res.has_value())
			return ERR(EPoll, bind_res.error());

		// Listen (연결 대기열 크기 128로 설정)
		Result<Void> listen_res = server_fd.socket_listen(128);
		if (!listen_res.has_value())
			return ERR(EPoll, listen_res.error());

		// EPoll에 등록할 이벤트 설정 (읽기 감지, Edge-Triggered 모드)
		Event ev(&server_fd, true, false, false, false, false, false); // in=true
		Option op(true, false, false, false);						   // et=true

		// EPoll에 소켓 추가 (add_fd는 소켓의 소유권을 가져가고, 내부 저장소의 포인터를 반환함)
		Result<const FileDescriptor *> add_res = epoll.add_fd(server_fd, ev, op);
		if (!add_res.has_value())
			return ERR(EPoll, add_res.error());

		// 반환된 포인터를 서버 소켓 목록에 저장 (나중에 클라이언트 소켓과 구분하기 위함)
		server_fds.insert(add_res.value());
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

					Result<const FileDescriptor *> add_res = epoll.add_fd(client_fd, client_ev, client_op);
					if (add_res.has_value())
					{
						const FileDescriptor *new_client_ptr = add_res.value();
						clients.insert(std::make_pair(new_client_ptr, ClientConnection(reinterpret_cast<intptr_t>(new_client_ptr))));
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
