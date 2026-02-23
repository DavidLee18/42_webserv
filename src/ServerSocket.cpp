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
		Result<Void> reuseaddr_result = server_fd.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (!reuseaddr_result.has_value())
			return ERR(EPoll, reuseaddr_result.error());

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
		Event event(&server_fd, true, false, false, false, false, false); // in=true
		Option op(true, false, false, false);						   // et=true

		// Add server socket to EPoll
		Result<const FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
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
		// Waiting for events
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

			// 1. Event occurred on server socket (new client connection)
			if (server_fds.find(fd) != server_fds.end())
			{
				while (true)
				{ // Must accept all pending connections since Edge-Triggered mode
					Result<FileDescriptor> client_res = fd->socket_accept(NULL, NULL);
					if (!client_res.has_value())
					{
						break; // EWOULDBLOCK: no more pending connections
					}
					FileDescriptor client_fd = client_res.value();
					auto nb_res = client_fd.set_nonblocking(); // Client socket must also be set to non-blocking
					if (!nb_res.has_value())
					{
						std::cerr << "ERROR: failed to set client socket to non-blocking mode" << std::endl;
						// skip this client; do not register with epoll
						continue;
					}

					// Register client socket with EPoll (read/write detection, Edge-Triggered)
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
			// 2. Event occurred on client socket (data send/receive)
			else
			{
				// Detect error or connection close
				if (event->err || event->hup || event->rdhup)
				{
					std::cout << "Client disconnected (error/hup)" << std::endl;
					FileDescriptor *client_fd = const_cast<FileDescriptor *>(fd);
					epoll.del_fd(*client_fd);
					client_fd->close(); // Close underlying socket to avoid FD leak
					clients.erase(fd);
					++events;
					continue;
				}

				// Read event (client sent data)
				if (event->in)
				{
					while (true)
					{ // Must read all available data since Edge-Triggered mode
						char buf[4096];
						Result<ssize_t> recv_res = fd->sock_recv(buf, sizeof(buf));
						if (!recv_res.has_value())
						{
							break; // EWOULDBLOCK: no more data to read
						}
						ssize_t bytes = recv_res.value();
						if (bytes == 0)
						{ // Client closed connection normally (EOF)
							std::cout << "Client disconnected (EOF)" << std::endl;
							epoll.del_fd(*const_cast<FileDescriptor *>(fd));
							clients.erase(fd);
							break;
						}
						// Store received data in buffer
						clients.at(fd).read_buffer.append(buf, static_cast<std::size_t>(bytes));
					}

					// TODO: call HTTP parsing logic here
					// Temporary: send a fixed response for any incoming data
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
						clients.at(fd).read_buffer.clear(); // Clear read buffer
					}
				}

				// Write event (can send data to client)
				if (event->out && clients.find(fd) != clients.end())
				{
					ClientConnection &client = clients.at(fd);
					if (!client.write_buffer.empty())
					{
						while (true)
						{ // Must send as much data as possible since Edge-Triggered mode
							Result<ssize_t> send_res = fd->sock_send(client.write_buffer.c_str(), client.write_buffer.length());
							if (!send_res.has_value())
							{
								break; // EWOULDBLOCK: socket buffer full, cannot send more
							}
							ssize_t bytes = send_res.value();
							if (bytes == 0)
							{
								break; // Cannot send more, exit loop to prevent infinite loop
							}
							client.write_buffer.erase(0, static_cast<std::size_t>(bytes)); // Remove sent bytes from buffer
							if (client.write_buffer.empty())
							{
								break; // All data sent
							}
						}
					}
				}
			}
			++events;
		}
	}
}
