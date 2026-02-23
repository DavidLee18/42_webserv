#include "ServerSocket.hpp"
<<<<<<< copilot/sub-pr-37-please-work
#include "errors.h"
=======
>>>>>>> sipyeon
#include <sstream>

// Remove a client from epoll and close the underlying file descriptor.
// Note: FileDescriptor::from_raw() transfers ownership; when tmp_fd goes out
// of scope its destructor calls close(raw_fd), releasing the file descriptor.
static void disconnect_client(EPoll &epoll, std::map<int, ClientConnection> &clients, int raw_fd)
{
	Result<FileDescriptor> tmp = FileDescriptor::from_raw(raw_fd);
	if (tmp.has_value())
	{
		FileDescriptor tmp_fd = tmp.value(); // Owns raw_fd; destructor will close it
		epoll.del_fd(tmp_fd);
	}
	clients.erase(raw_fd);
}

Result<EPoll> init_servers(const WebserverConfig &config, std::set<int> &server_fds)
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

		// Non-blocking socket for ET (edge-triggered)
		Result<Void> nb_result = server_fd.set_nonblocking();
		if (!nb_result.has_value())
			return ERR(EPoll, nb_result.error());

		// Port reusing option
		int opt = 1;
		Result<Void> reuseaddr_result = server_fd.set_socket_option(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (!reuseaddr_result.has_value())
			return ERR(EPoll, reuseaddr_result.error());

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
		Option op(true, false, false, false);						   // et=true

		// Add server socket to EPoll
		Result<const FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
		if (!add_result.has_value())
			return ERR(EPoll, add_result.error());

		// Store raw fd (int) to avoid dangling pointer if EPoll's vector reallocates
		server_fds.insert(add_result.value()->get_fd());
		std::cout << "Server listening on port " << port << std::endl;
	}

	return OK(EPoll, epoll);
}

void run_server(EPoll &epoll, const std::set<int> &server_fds)
{
	std::map<int, ClientConnection> clients;

	while (true)
	{
		// Wait for events
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
			int raw_fd = event->fd->get_fd();

			// 1. Event on a server socket (new client connection)
			if (server_fds.find(raw_fd) != server_fds.end())
			{
				while (true)
				{ // Because EPoll is edge-triggered, we must accept all pending connections
					Result<FileDescriptor> client_res = event->fd->socket_accept(NULL, NULL);
					if (!client_res.has_value())
					{
						const std::string &err = client_res.error();
						if (err == Errors::try_again)
						{
							// EWOULDBLOCK: no more pending connections
							break;
						}
						else if (err == Errors::interrupted)
						{
							// EINTR: retry accept
							continue;
						}
						else
						{
							// Other error: log and stop accepting for this event
							std::cerr << "ERROR: accept failed: " << err << std::endl;
							break;
						}
					}
					FileDescriptor client_fd = client_res.value();
					// Client socket must also be non-blocking for edge-triggered mode
					Result<Void> nb_res = client_fd.set_nonblocking();
					if (!nb_res.has_value())
					{
						std::cerr << "ERROR: failed to set client socket to non-blocking mode" << std::endl;
						// skip this client; do not register with epoll
						continue;
					}

					// Register client socket in EPoll (monitor read/write, edge-triggered)
					Event client_ev(&client_fd, true, true, false, false, false, false); // in=true, out=true
					Option client_op(true, false, false, false);						 // et=true (edge-triggered)

					Result<const FileDescriptor *> add_result = epoll.add_fd(client_fd, client_ev, client_op);
					if (add_result.has_value())
					{
						int new_client_fd = add_result.value()->get_fd();
						clients.insert(std::make_pair(new_client_fd, ClientConnection(new_client_fd)));
						std::cout << "New client connected!" << std::endl;
					}
				}
			}
			// 2. Event on a client socket (data send/receive)
			else
			{
				// Detect error or connection close
				if (event->err || event->hup || event->rdhup)
				{
					std::cout << "Client disconnected (error/hup)" << std::endl;
<<<<<<< copilot/sub-pr-37-please-work
					disconnect_client(epoll, clients, raw_fd);
=======
					epoll.del_fd(*const_cast<FileDescriptor *>(fd));
					clients.erase(fd);
>>>>>>> sipyeon
					++events;
					continue;
				}

				// Read event (client sent data)
				if (event->in)
				{
					while (true)
					{ // Because EPoll is edge-triggered, we must read all available data
						char buf[4096];
						Result<ssize_t> recv_res = event->fd->sock_recv(buf, sizeof(buf));
						if (!recv_res.has_value())
						{
<<<<<<< copilot/sub-pr-37-please-work
							break; // EWOULDBLOCK: no more data to read
=======
							if (recv_res.error() == Errors::try_again)
								break; // EAGAIN/EWOULDBLOCK: 더 이상 읽을 데이터 없음
							// Real recv error (e.g. ECONNRESET): close and remove client
							std::cerr << "Client recv error: " << recv_res.error() << std::endl;
							epoll.del_fd(*const_cast<FileDescriptor *>(fd));
							clients.erase(fd);
							break;
>>>>>>> sipyeon
						}
						ssize_t bytes = recv_res.value();
						if (bytes == 0)
						{ // Client closed connection cleanly (EOF)
							std::cout << "Client disconnected (EOF)" << std::endl;
							disconnect_client(epoll, clients, raw_fd);
							break;
						}
						// Store received data in the read buffer
						clients.at(raw_fd).read_buffer.append(buf, static_cast<std::size_t>(bytes));
					}

					// TODO: call HTTP parsing logic here
					// For now, send a fixed response whenever data is received
					if (clients.find(raw_fd) != clients.end() && !clients.at(raw_fd).read_buffer.empty())
					{
						std::string body = "<html><body><h1>Hello from webserv!</h1></body></html>";
						std::ostringstream response;
						response << "HTTP/1.1 200 OK\r\n";
						response << "Content-Type: text/html\r\n";
						response << "Content-Length: " << body.length() << "\r\n";
						response << "Connection: keep-alive\r\n\r\n";
						response << body;

						clients.at(raw_fd).write_buffer = response.str();
						clients.at(raw_fd).read_buffer.clear(); // Clear the read buffer
					}
				}

				// Write event (socket ready to send data to client)
				if (event->out && clients.find(raw_fd) != clients.end())
				{
					ClientConnection &client = clients.at(raw_fd);
					if (!client.write_buffer.empty())
					{
						while (true)
						{ // Because EPoll is edge-triggered, send as much as possible
							Result<ssize_t> send_res = event->fd->sock_send(client.write_buffer.c_str(), client.write_buffer.length());
							if (!send_res.has_value())
							{
								break; // EWOULDBLOCK: socket send buffer is full
							}
							ssize_t bytes = send_res.value();
							if (bytes == 0)
							{
								break; // No bytes sent; prevent infinite loop
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

