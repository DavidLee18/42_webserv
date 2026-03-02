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
	std::map<const FileDescriptor *, ClientSession> clients;

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

		// Handling events
		Events events = events_result.value();
		while (!events.is_end()) {
			Result<const Event *> ev_result = *events;
			if (!ev_result.has_value()) {
				++events;
				continue;
			}
			const Event *event = ev_result.value();

			const FileDescriptor *fd = event->fd;
			// New event in server socket (New client)
			if (server_fds.find(fd) != server_fds.end()) {
				while (true) { // Have to search for every clients due to Edge-Triggered
					Result<FileDescriptor> client_result = fd->socket_accept(NULL, NULL);
					if (!client_result.has_value()) {
						const std::string &err = client_result.error();
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
					FileDescriptor client_fd = client_result.value();
					Result<Void> nb_res = client_fd.set_nonblocking();
					if (!nb_res.has_value()) {
						std::cerr << "ERROR: failed to set client socket to non-blocking mode" << std::endl;
						// Skip this client; do not register with epoll
						continue;
					}

					// Register client socket with EPoll (read/write detection, Edge-Triggered)
					Event client_event(&client_fd, true, true, false, false, false, false); // in=true, out=true
					Option client_option(true, false, false, false);                         // et=true (edge-triggered)

					Result<FileDescriptor *> add_result = epoll.add_fd(client_fd, client_event, client_option);
					if (add_result.has_value()) {
						FileDescriptor *client_ptr = add_result.value();
						clients[client_ptr] = ClientSession();
						std::cout << "New client connected!" << std::endl;
					} else {
						std::cerr << "ERROR: failed to add client fd to epoll: " << add_result.error() << std::endl;
						// client_fd goes out of scope here and is closed via RAII
					}
				}
			}
			// Event occured by client (data I/O)
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
						clients.at(fd).in_buff.append(buf, static_cast<std::size_t>(bytes));
					}

					// TODO: 여기서 HTTP 파싱 로직 호출
					// 임시로, 데이터가 들어오면 무조건 고정된 응답을 보내도록 설정
					if (clients.find(fd) != clients.end() && !clients.at(fd).in_buff.empty()) {
						std::string &in_buffer = clients.at(fd).in_buff;

						// 1. HTTP 헤더가 모두 도착했는지 확인 (\r\n\r\n)
						// (Non-blocking이라 데이터가 반쪽만 왔을 수도 있으므로 반드시 확인해야 합니다)
						size_t header_end = in_buffer.find("\r\n\r\n");

						if (header_end != std::string::npos) { // 요청의 헤더 끝부분을 찾았다면 파싱 시작
							
							// 2. 가장 첫 줄(요청 라인)만 잘라내기
							// 예: "GET /index.html HTTP/1.1"
							size_t first_line_end = in_buffer.find("\r\n");
							std::string request_line = in_buffer.substr(0, first_line_end);

							// 3. 공백을 기준으로 Method와 Path 파싱하기 stringstream 이용
							std::istringstream iss(request_line);
							std::string method, path, version;
							iss >> method >> path >> version;

							// 터미널에 어떤 요청이 왔는지 출력
							std::cout << "[Request] " << method << " " << path << std::endl;

							std::string body;
							std::string status_code;

							// 4. Path(경로)에 따라 응답을 다르게 분기 처리 (라우팅)
							if (path == "/") {
								std::ifstream file("./spool/www/index.html");
								if (file.is_open())
								{
									std::ostringstream ss;
									ss << file.rdbuf();
									body = ss.str();
									status_code = "200 OK";
									file.close();
								}
							} 
							else if (path == "/hello") {
								status_code = "200 OK";
								body = "<html><body><h1>Hello! Nice to meet you.</h1></body></html>";
							} 
							else {
								// 정의되지 않은 경로면 404 에러 반환
								status_code = "404 Not Found";
								body = "<html><body><h1>404 Error: File Not Found</h1></body></html>";
							}

							// 5. 클라이언트에게 보낼 HTTP 응답 메시지 조립
							std::ostringstream response;
							response << "HTTP/1.1 " << status_code << "\r\n"; //todo: status code
							response << "Content-Type: text/html\r\n"; //todo: mimetype 심기
							response << "Content-Length: " << body.length() << "\r\n";
							response << "Connection: keep-alive\r\n\r\n";
							response << body;

							// 6. 만들어진 응답을 송신 버퍼(out_buff)에 넣기
							clients.at(fd).out_buff += response.str();

							// 7. 파싱이 끝난 헤더 부분은 수신 버퍼에서 삭제 
							// (+4는 "\r\n\r\n" 문자열의 길이입니다)
							in_buffer.erase(0, header_end + 4); 
						}
					}
				}

				// 쓰기 이벤트 (클라이언트에게 데이터를 보낼 수 있음)
				if (event->out && clients.find(fd) != clients.end()) {
					std::string &write_buffer = clients.at(fd).out_buff;
					if (!write_buffer.empty()) {
						while (true) { // Edge-Triggered이므로 보낼 수 있는 만큼 다 보내야 함
							Result<ssize_t> send_res = fd->sock_send(write_buffer.c_str(), write_buffer.length());
							if (!send_res.has_value()) {
								break; // EWOULDBLOCK 등: 소켓 버퍼가 꽉 차서 더 못 보냄
							}
							ssize_t bytes = send_res.value();
							if (bytes == 0) {
								// sock_send()가 0을 반환하면 더 이상 보낼 수 없으므로,
								// 더 진행해도 진전이 없고 무한 루프가 될 수 있어 루프를 종료한다.
								break;
							}
							write_buffer.erase(0, static_cast<std::size_t>(bytes)); // 보낸 만큼 버퍼에서 삭제
							if (write_buffer.empty()) {
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

