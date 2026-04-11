#include "Server.hpp"
#include "../webserv.h"

void Server::new_connection(const FileDescriptor *server_fd) {
  while (true) { // accept all clients until nothing to connect
    // init client socket
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    Result<FileDescriptor> client_result = server_fd->socket_accept((struct sockaddr*)&client_addr, &client_len);
    if (!client_result.has_value()) {
      const std::string &err = client_result.error();
      if (err == Errors::try_again)
        break; // EWOULDBLOCK: nothing to connect
      else if (err == Errors::interrupted)
        continue; // EINTR: accept retry
      else {
        std::cerr << "ERROR: accept failed: " << err << std::endl;
        break;
      }
    }
    FileDescriptor client_fd = client_result.value();
    if (!client_fd.set_nonblocking().has_value()) {
      std::cerr << "ERROR: failed to set client socket to non-blocking mode"
                << std::endl;
      continue;
    }

    ClientSession client;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
    client.ip = ip_str;

    // register client socket to EPoll
    Event client_event(&client_fd, true, true, false, false, false, false);
    Option client_option(true, false, false, false);

    Result<FileDescriptor *> add_result =
        epoll.add_fd(client_fd, client_event, client_option);
    if (add_result.has_value()) {
      FileDescriptor *client_ptr = add_result.value();
      if (listeners.find(server_fd) != listeners.end()) {
        client.config = listeners.at(server_fd);
      }
      clients[client_ptr] = client;
      std::cout << "New client connected!" << std::endl;
    } else {
      std::cerr << "ERROR: epoll add failed: " << add_result.error()
                << std::endl;
    }
  }
}

void Server::disconnect(const FileDescriptor *client_fd) {
  std::cout << "Client disconnected" << std::endl;
  epoll.del_fd(*client_fd);
  clients.erase(client_fd);
}

void Server::client_read(const FileDescriptor *client_fd) {
  while (true) { // repeat until nothing to read
    char buf[NETWORK_BUFFER_SIZE];
    Result<ssize_t> recv_res = client_fd->sock_recv(buf, sizeof(buf));
    if (!recv_res.has_value())
      break; // EWOULDBLOCK

    ssize_t bytes = recv_res.value();
    if (bytes == 0) { // (EOF)
      disconnect(client_fd);
      return;
    }
    clients.at(client_fd).in_buff.append(buf, static_cast<std::size_t>(bytes));
  }

  // HTTP parsing and response generate
  std::string &in_buffer = clients.at(client_fd).in_buff;
  while (!in_buffer.empty()) {
    size_t header_end = in_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      break; // 헤더가 다 안 들어왔으면 다음 epoll 이벤트 대기
    }

    // checking Content-Length
    size_t content_length = 0;
    std::string header_lower = in_buffer.substr(0, header_end);
    for (size_t i = 0; i < header_lower.length(); ++i) {
      header_lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(header_lower[i])));
    }
    size_t cl_pos = header_lower.find("content-length: ");
    if (cl_pos != std::string::npos) {
      content_length = std::atoi(header_lower.c_str() + cl_pos + 16);
    }

    // check body if body not full break to get more event
    size_t total_request_len = header_end + 4 + content_length;
    if (in_buffer.length() < total_request_len) {
      break;
    }

    // 완벽히 조립된 단일 HTTP 요청 문자열 잘라내기
    std::string request_str = in_buffer.substr(0, total_request_len);
    Request request(request_str);

    std::cout << "\nclient ip: " << clients.at(client_fd).ip << std::endl;
    std::cout << "[Request] " << request.get_method_string() << " "
              << request.get_path() << " (Body: " << content_length << " bytes)"
              << std::endl;

    // response generate
    Response http;
    if (ServerResponse::find_file_type(request.get_path()) == "cgi")
      http = ServerResponse::cgi_response(&request,
                                          clients.at(client_fd).config, &epoll);
    else
      http = ServerResponse::http_response(
          &request, &clients.at(client_fd), mime_type, &sessions);

    // read server response
    std::ostringstream server_response;
    if (!http.cgi.empty())
      server_response << http.cgi;
    else {
      server_response << "HTTP/1.1 " << http.status_code << "\r\n";
      if (!http.redir.empty())
        server_response << "Location: " << http.redir << "\r\n";
      std::cout << http.redir << std::endl;
      server_response << "Content-Type:" << http.mime_type << "\r\n";
      if (!http.cookie.empty())
      {
        server_response << "Set-Cookie:" << http.cookie << "\r\n";
        std::cout << "cookie value: " << http.cookie << std::endl;
      }
      server_response << "Content-Length: " << http.body.length() << "\r\n";
      server_response << "Connection: " << http.connection << "\r\n\r\n";
      server_response << http.body;
    }
    clients.at(client_fd).out_buff += server_response.str();

    in_buffer.erase(0, total_request_len);
  }
}

void Server::client_write(const FileDescriptor *client_fd) {
  if (clients.find(client_fd) == clients.end())
    return;

  std::string &write_buffer = clients.at(client_fd).out_buff;
  if (!write_buffer.empty()) {
    while (true) { // ET 모드이므로 보낼 수 있는 만큼 다 보냄
      Result<ssize_t> send_res =
          client_fd->sock_send(write_buffer.c_str(), write_buffer.length());
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

Result<Void> Server::init() {
  // EPoll init
  Result<EPoll> epoll_result = EPoll::create(1024);
  if (!epoll_result.has_value())
    return ERR(Void, "Epoll create fail: " + epoll_result.error());
  epoll = epoll_result.value();

  // Init server socket for every port listed on configuration file
  const std::map<unsigned int, ServerConfig> &servers =
      config.get_serverconfig_map();
  for (std::map<unsigned int, ServerConfig>::const_iterator it =
           servers.begin();
       it != servers.end(); ++it) {
    unsigned short port = static_cast<unsigned short>(it->first);

    // Init socket
    Result<FileDescriptor> sock_result = FileDescriptor::socket_new();
    if (!sock_result.has_value())
      return ERR(Void, "Socket fail: " + sock_result.error());
    FileDescriptor server_fd = sock_result.value();

    // Non-blocking socket for ET (edge-triggered)
    Result<Void> nb_result = server_fd.set_nonblocking();
    if (!nb_result.has_value())
      return ERR(Void, "set nonblocking fail: " + nb_result.error());

    // Port reusing option
    int opt = 1;
    Result<Void> reuseaddr_result = server_fd.set_socket_option(
        SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (!reuseaddr_result.has_value())
      std::cerr << "WARNING: SO_REUSEADDR failed: " << reuseaddr_result.error()
                << std::endl;

    // Bind (associate IP and port)
    struct in_addr addr;
    addr.s_addr = htonl(INADDR_ANY); // All IPs
    Result<Void> bind_result = server_fd.socket_bind(addr, port);
    if (!bind_result.has_value())
      return ERR(Void, "Bind fail: " + bind_result.error());

    // Listen (max queue length)
    Result<Void> listen_result = server_fd.socket_listen(SOMAXCONN);
    if (!listen_result.has_value())
      return ERR(Void, "Listen fail: " + listen_result.error());

    // EPoll event and option setting
    Event event(&server_fd, true, false, false, false, false, false); // in=true
    Option op(true, false, false, false);                             // et=true

    // Add server socket to EPoll
    Result<FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
    if (!add_result.has_value())
      return ERR(Void, "Server register fail: " + add_result.error());

    // Save pointer to distinguish server sockets from client sockets
    FileDescriptor *fd_ptr = add_result.value();
    listeners[fd_ptr] = &it->second;

    std::cout << "Server listening " << inet_ntoa(addr) << " : " << port
              << std::endl;
  }
  return OK(Void, Void());
}

Result<Void> Server::start() {
  system("open http://localhost:8080");
  std::cout << "Starting server loop..." << std::endl;
  while (true) {
    // Waiting for events using epoll
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

      // 1. 서버 소켓(문지기)인 경우 (listeners map에 Key가 존재함)
      if (listeners.find(fd) != listeners.end()) {
        new_connection(fd);
      }
      // 2. 이미 연결된 클라이언트 소켓인 경우
      else {
        if (event->err || event->hup || event->rdhup) {
          disconnect(fd);
        } else {
          if (event->in)
            client_read(fd);
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
