#include "webserv.h"
#include "ServerSocket.hpp"

Result<EPoll> init_servers(const WebserverConfig &config, std::set<const FileDescriptor *> &server_fds) {
	// EPoll init
	Result<EPoll> epoll_result = EPoll::create(1024);
	if (!epoll_result.has_value())
		return epoll_result;
	EPoll epoll = epoll_result.value();

	// Init server socket for every port listed on configuration file
	const std::map<unsigned int, ServerConfig> &servers = config.Get_ServerConfig_map();
	for (std::map<unsigned int, ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		unsigned short port = static_cast<unsigned short>(it->first);

		// Init socket
		Result<FileDescriptor> sock_result = FileDescriptor::socket_new();
		if (!sock_result.has_value())
			return ERR(EPoll, sock_result.error());
		FileDescriptor server_fd = sock_result.value();

		// Non-blocking socket for ET (edge-triggered)
		Result<Void> nb_result = server_fd.set_nonblocking();
		if (!nb_result.has_value())
			return ERR(EPoll, nb_result.error());

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
			return ERR(EPoll, bind_result.error());

		// Listen (max queue length)
		Result<Void> listen_result = server_fd.socket_listen(SOMAXCONN);
		if (!listen_result.has_value())
			return ERR(EPoll, listen_result.error());

		// EPoll event and option setting
		Event event(&server_fd, true, false, false, false, false, false); // in=true
		Option op(true, false, false, false);                              // et=true

		// Add server socket to EPoll
		Result<FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
		if (!add_result.has_value())
			return ERR(EPoll, add_result.error());

		// Save pointer to distinguish server sockets from client sockets
		server_fds.insert(add_result.value());
		std::cout << "Server listening on port " << port << std::endl;
	}

	return OK(EPoll, epoll);
}

void run_server(EPoll &epoll, const std::set<const FileDescriptor *> &server_fds) {
	std::map<const FileDescriptor *, ClientConnection> clients;

	while (true) {
		// Waiting for events
		Result<Events> events_result = epoll.wait(-1);
		if (!events_result.has_value()) {
			if (events_result.error() == Errors::interrupted)
				continue;
			else {
				std::cerr << "ERROR: " << events_result.error() << std::endl;
				break;
			}
		}

		Events events = events_result.value();
		while (!events.is_end()) {
			Result<const Event *> ev_result = *events;
			if (!ev_result.has_value()) {
				++events;
				continue;
			}
			const Event *event = ev_result.value();
			const FileDescriptor *fd = event->fd;

			// 1. 서버 소켓에 이벤트가 발생한 경우 (새로운 클라이언트 접속)
			if (server_fds.find(fd) != server_fds.end()) {
				while (true) { // Edge-Triggered이므로 가능한 모든 연결을 accept 해야 함
					Result<FileDescriptor> client_res = fd->socket_accept(NULL, NULL);
					if (!client_res.has_value()) {
						const std::string &err = client_res.error();
						if (err == Errors::try_again)
						{
							// EWOULDBLOCK: 더 이상 대기 중인 연결이 없음
							break;
						}
						else if (err == Errors::interrupted)
						{
							// EINTR: accept 재시도
							continue;
						}
						else
						{
							// 기타 오류: 로그만 남기고 해당 이벤트에 대한 accept 루프 종료
							std::cerr << "ERROR: accept failed: " << err << std::endl;
							break;
						}
					}
					FileDescriptor client_fd = client_res.value();
					Result<Void> nb_res = client_fd.set_nonblocking(); // 클라이언트 소켓도 논블로킹 필수!
					if (!nb_res.has_value()) {
						std::cerr << "ERROR: failed to set client socket to non-blocking mode" << std::endl;
						// Skip this client; do not register with epoll
						continue;
					}

					// Register client socket with EPoll (read/write detection, Edge-Triggered)
					Event client_ev(&client_fd, true, true, false, false, false, false); // in=true, out=true
					Option client_op(true, false, false, false);                         // et=true (edge-triggered)

					Result<FileDescriptor *> add_result = epoll.add_fd(client_fd, client_ev, client_op);
					if (add_result.has_value()) {
						FileDescriptor *client_ptr = add_result.value();
						clients.insert(std::make_pair(client_ptr,
												ClientConnection(client_ptr->raw_fd())));
						std::cout << "New client connected!" << std::endl;
					} else {
						std::cerr << "ERROR: failed to add client fd to epoll: " << add_result.error() << std::endl;
						// client_fd goes out of scope here and is closed via RAII
					}
				}
			}
			// 2. 클라이언트 소켓에 이벤트가 발생한 경우 (데이터 송수신)
			else {
				// 에러나 연결 종료 감지
				if (event->err || event->hup || event->rdhup) {
					std::cout << "Client disconnected (error/hup)" << std::endl;
					epoll.del_fd(*fd);
					clients.erase(fd);
					++events;
					continue;
				}

				// 읽기 이벤트 (클라이언트가 데이터를 보냄)
				if (event->in) {
					while (true) { // Edge-Triggered이므로 모든 데이터를 읽어야 함
						char buf[NETWORK_BUFFER_SIZE];
						Result<ssize_t> recv_res = fd->sock_recv(buf, sizeof(buf));
						if (!recv_res.has_value()) {
							break; // EWOULDBLOCK: 더 이상 읽을 데이터 없음
						}
						ssize_t bytes = recv_res.value();
						if (bytes == 0) { // 클라이언트가 정상적으로 연결 종료 (EOF)
							std::cout << "Client disconnected (EOF)" << std::endl;
							epoll.del_fd(*fd);
							clients.erase(fd);
							break;
						}
						// Store read data in buffer
						clients.at(fd).read_buffer.append(buf, static_cast<std::size_t>(bytes));
					}

					// TODO: 여기서 HTTP 파싱 로직 호출
					// 임시로, 데이터가 들어오면 무조건 고정된 응답을 보내도록 설정
					if (clients.find(fd) != clients.end() && !clients.at(fd).read_buffer.empty()) {
						std::string body = "<html><body><h1>Hello from webserv!</h1></body></html>";
						std::ostringstream response;
						response << "HTTP/1.1 200 OK\r\n";
						response << "Content-Type: text/html\r\n";
						response << "Content-Length: " << body.length() << "\r\n";
						response << "Connection: keep-alive\r\n\r\n";
						response << body;

						clients.at(fd).write_buffer = response.str();
						clients.at(fd).read_buffer.clear(); // Clear read buffer
					}
				}

				// 쓰기 이벤트 (클라이언트에게 데이터를 보낼 수 있음)
				if (event->out && clients.find(fd) != clients.end()) {
					ClientConnection &client = clients.at(fd);
					if (!client.write_buffer.empty()) {
						while (true) { // Edge-Triggered이므로 보낼 수 있는 만큼 다 보내야 함
							Result<ssize_t> send_res = fd->sock_send(client.write_buffer.c_str(), client.write_buffer.length());
							if (!send_res.has_value()) {
								break; // EWOULDBLOCK 등: 소켓 버퍼가 꽉 차서 더 못 보냄
							}
							ssize_t bytes = send_res.value();
							if (bytes == 0) {
								// sock_send()가 0을 반환하면 더 이상 보낼 수 없으므로,
								// 더 진행해도 진전이 없고 무한 루프가 될 수 있어 루프를 종료한다.
								break;
							}
							client.write_buffer.erase(0, static_cast<std::size_t>(bytes)); // 보낸 만큼 버퍼에서 삭제
							if (client.write_buffer.empty()) {
								break; // 다 보냈음
							}
						}
					}
				}
			}
			++events;
		}
	}

	// Graceful shutdown: close all client connections
	// client file descriptors are owned by EPoll and will be closed when epoll goes out of scope
	clients.clear();
}

