#include "Server.hpp"
#include "../webserv.h"
#include "Response.hpp"
#include <cstddef>
#include <ctime>
#include <vector>

void Server::new_connection(const FileDescriptor *server_fd) {
  while (true) { // accept all clients until nothing to connect
    // init client socket
    sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);
    Result<FileDescriptor> client_result = server_fd->socket_accept(
        reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
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

    if (!client_fd.close_on_exec().has_value()) {
      std::cerr << "ERROR: failed to set client socket to close-on-exec mode"
                << std::endl;
      continue;
    }

    ClientSession client;
    if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
      std::cerr << "ERROR: failed to get current time for client activity"
                << std::endl;
      continue;
    }
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN) ==
        NULL) {
      std::cerr << "ERROR: failed to convert client IP address to string"
                << std::endl;
      continue;
    }
    client.ip = ip_str;

    // register client socket to EPoll
    Event client_event(client_fd, true, true, true, false, false, false);
    Option client_option(true, false, false, false);

    Result<FileDescriptor *> add_result =
        epoll.add_fd(client_fd, client_event, client_option);
    if (add_result.has_value()) {
      const FileDescriptor *client_ptr = add_result.value();
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
  epoll.del_fd(client_fd);
  clients.erase(client_fd);
}

void Server::client_read(const FileDescriptor *client_fd) {
  if (clients.find(client_fd) == clients.end()) {
    return;
  }
  if (clock_gettime(CLOCK_MONOTONIC,
                    &clients.at(client_fd).last_activity_time) != 0) {
    std::cerr << "ERROR: failed to update client activity time" << std::endl;
    return;
  }
  while (true) { // repeat until nothing to read
    char buf[NETWORK_BUFFER_SIZE];
    Result<ssize_t> recv_res = client_fd->sock_recv(buf, sizeof(buf));
    if (!recv_res.has_value())
      break; // EWOULDBLOCK

    ssize_t bytes = recv_res.value();
    if (bytes == 0) { // (EOF)
      clients.at(client_fd).dropping = true;
      break;
    }
    clients.at(client_fd).in_buff.append(buf, static_cast<std::size_t>(bytes));
  }

  // HTTP parsing and response generate
  std::string &in_buffer = clients.at(client_fd).in_buff;
  while (!in_buffer.empty()) {
    if (clients.at(client_fd).req == NULL) {
      Result<Request *> req_ = Request::from_buff(in_buffer);

      if (!req_.has_value()) {
        std::cerr << "request parsing failed: " << req_.error() << std::endl;
        if (req_.error() == Errors::incomplete_header) {
          if (clients.at(client_fd).dropping)
            disconnect(client_fd);
          return;
        } else if (req_.error() == Errors::malformed_header ||
                   req_.error() == Errors::bad_request) {
          Response resp(
              DefaultError::default_err_response(Response::BAD_REQUEST));
          resp.headers = clients.at(client_fd).config->get_header();
          std::ostringstream oss;
          oss << resp;
          if (!resp.keep_alive)
            clients.at(client_fd).dropping = true;
          clients.at(client_fd).out_buff += oss.str();
          client_write(client_fd); // when the response is generated freshly,
                                   // likely EPOLLIN | EPOLLOUT

          if (clients.find(client_fd) != clients.end() &&
              clients.at(client_fd).dropping &&
              clients.at(client_fd).out_buff.empty())
            disconnect(client_fd);

          return;
        } else if (req_.error() == Errors::not_implemented) {
          Response resp =
              DefaultError::default_err_response(Response::NOT_IMPLEMENTED);
          resp.headers = clients.at(client_fd).config->get_header();
          std::ostringstream oss;
          oss << resp;
          if (!resp.keep_alive)
            clients.at(client_fd).dropping = true;
          clients.at(client_fd).out_buff += oss.str();
          client_write(client_fd); // when the response is generated freshly,
                                   // likely EPOLLIN | EPOLLOUT

          if (clients.at(client_fd).dropping &&
              clients.at(client_fd).out_buff.empty())
            disconnect(client_fd);

          return;
        }
      }

      clients.at(client_fd).req = req_.value();
      if (clients.at(client_fd).req->is_partial()) // 아직 파싱 더 해야함
      {
        if (clients.at(client_fd).dropping)
          disconnect(client_fd);
        return;
      }

      Result<size_t> content_length =
          clients.at(client_fd).req->get_content_length();

      // 완벽히 조립된 단일 HTTP 요청 문자열 잘라내기
      std::cout << "\nclient ip: " << clients.at(client_fd).ip << std::endl;
      std::cout << "[Request] "
                << clients.at(client_fd).req->get_method_string() << " "
                << clients.at(client_fd).req->get_path() << " (Body: ";
      if (content_length.has_value())
        std::cout << content_length.value();
      else
        std::cout << "(non-existent)";
      std::cout << " bytes)" << std::endl;

      RouteRule_CGI const *cgi_path =
          clients.at(client_fd).config->find_route_cgi(
              clients.at(client_fd).req->get_method(),
              clients.at(client_fd).req->get_path());
      if (cgi_path != NULL) {
        Result<Void> del_ = ServerResponse::register_cgi(
            *clients.at(client_fd).req, *cgi_path, &epoll, cgis, client_fd);
        if (!del_.has_value())
          std::cerr << "CGI registration failed: " << del_.error() << std::endl;
        delete clients.at(client_fd).req;
        clients.at(client_fd).req = NULL;
        return;
      }
      // response generate
      Response http = ServerResponse::http_response(clients.at(client_fd).req,
                                                    &clients.at(client_fd),
                                                    mime_type, &sessions);

      // read server response
      std::ostringstream server_response;

      server_response << http;

      delete clients.at(client_fd).req;
      clients.at(client_fd).req = NULL;

      clients.at(client_fd).out_buff += server_response.str();

      // If client sent "Connection: close", close after sending response
      if (!http.keep_alive) {
        client_write(client_fd);
        if (clients.find(client_fd) != clients.end() &&
            clients.at(client_fd).out_buff.empty())
          disconnect(client_fd);
        return;
      }
    } else {
      clients.at(client_fd).req->continue_parsing(in_buffer);
      if (clients.at(client_fd).req->is_partial()) // 아직 파싱 더 해야함
        return;

      const Result<size_t> content_length =
          clients.at(client_fd).req->get_content_length();

      // 완벽히 조립된 단일 HTTP 요청 문자열 잘라내기
      std::cout << "\nclient ip: " << clients.at(client_fd).ip << std::endl;
      std::cout << "[Request] "
                << clients.at(client_fd).req->get_method_string() << " "
                << clients.at(client_fd).req->get_path() << " (Body: ";
      if (content_length.has_value())
        std::cout << content_length.value();
      else
        std::cout << "(non-existent)";
      std::cout << " bytes)" << std::endl;

      RouteRule_CGI const *cgi_path =
          clients.at(client_fd).config->find_route_cgi(
              clients.at(client_fd).req->get_method(),
              clients.at(client_fd).req->get_path());

      if (cgi_path != NULL) {
        Result<Void> del_ = ServerResponse::register_cgi(
            *clients.at(client_fd).req, *cgi_path, &epoll, cgis, client_fd);
        if (!del_.has_value())
          std::cerr << "CGI registration failed: " << del_.error() << std::endl;
        delete clients.at(client_fd).req;
        clients.at(client_fd).req = NULL;
        return;
      }
      // response generate
      Response http = ServerResponse::http_response(clients.at(client_fd).req,
                                                    &clients.at(client_fd),
                                                    mime_type, &sessions);

      // read server response
      std::ostringstream server_response;

      server_response << http;

      delete clients.at(client_fd).req;
      clients.at(client_fd).req = NULL;

      clients.at(client_fd).out_buff += server_response.str();

      // If client sent "Connection: close", close after sending response
      if (!http.keep_alive) {
        clients.at(client_fd).dropping = true;
        client_write(client_fd);
        return;
      }
    }
  }
  client_write(client_fd); // when the response is generated freshly, likely
                           // EPOLLIN | EPOLLOUT

  if (clients.find(client_fd) != clients.end() &&
      clients.at(client_fd).dropping &&
      clients.at(client_fd).out_buff.empty()) {
    disconnect(client_fd);
  }
}

void Server::client_write(const FileDescriptor *client_fd) {
  if (clients.find(client_fd) == clients.end())
    return;
  if (clock_gettime(CLOCK_MONOTONIC,
                    &clients.at(client_fd).last_activity_time) != 0) {
    std::cerr << "ERROR: failed to update client activity time" << std::endl;
    return;
  }
  std::string &write_buffer = clients.at(client_fd).out_buff;
  if (!write_buffer.empty()) {
    while (true) { // ET 모드이므로 보낼 수 있는 만큼 다 보냄
      Result<ssize_t> send_res =
          client_fd->sock_send(write_buffer.c_str(), write_buffer.length());
      if (!send_res.has_value())
        break; // EWOULDBLOCK

      const ssize_t bytes = send_res.value();
      if (bytes == 0)
        break;

      write_buffer.erase(0, static_cast<std::size_t>(bytes));
      if (write_buffer.empty()) {
        if (clients.at(client_fd).dropping)
          disconnect(client_fd);
        return;
      }
    }
  } else if (clients.at(client_fd).dropping)
    disconnect(client_fd);
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

    Result<Void> close_on_exec_result = server_fd.close_on_exec();
    if (!close_on_exec_result.has_value())
      return ERR(Void, "close on exec fail: " + close_on_exec_result.error());

    // Port reusing option
    int opt = 1;
    Result<Void> reuseaddr_result = server_fd.set_socket_option(
        SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (!reuseaddr_result.has_value())
      return ERR(Void, "SO_REUSEADDR failed: " + reuseaddr_result.error());

    // Bind (associate IP and port)
    in_addr addr = {};
    addr.s_addr = htonl(INADDR_ANY); // All IPs
    Result<Void> bind_result = server_fd.socket_bind(addr, port);
    if (!bind_result.has_value())
      return ERR(Void, "Bind fail: " + bind_result.error());

    // Listen (max queue length)
    Result<Void> listen_result = server_fd.socket_listen(SOMAXCONN);
    if (!listen_result.has_value())
      return ERR(Void, "Listen fail: " + listen_result.error());

    // EPoll event and option setting
    Event event(server_fd, true, false, false, false, false, false); // in=true
    Option op(true, false, false, false);                            // et=true

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
  // system("open http://localhost:8080");
  std::cout << "Starting server loop..." << std::endl;
  long epoll_timeout = -1; // Default: wait indefinitely
  while (g_receivedSignal == 0) {
    // Check for client timeouts and calculate epoll timeout
    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      std::cerr << "ERROR: failed to get current time for client timeout check"
                << std::endl;
      continue;
    }

    // Collect clients to disconnect (avoid modifying map during iteration)
    std::vector<const FileDescriptor *> clients_to_disconnect;

    for (std::map<const FileDescriptor *, ClientSession>::iterator it =
             clients.begin();
         it != clients.end(); ++it) {
      const FileDescriptor *client_fd = it->first;
      const ClientSession &session = it->second;

      if (session.config == NULL)
        continue;

      const long timeout_sec =
          static_cast<long>(session.config->get_server_response_time());
      if (timeout_sec > 0) {
        const timespec elapsed = {
            .tv_sec = now.tv_sec - session.last_activity_time.tv_sec,
            .tv_nsec = now.tv_nsec - session.last_activity_time.tv_nsec};
        if (elapsed.tv_sec < 0 ||
            (elapsed.tv_sec == 0 && elapsed.tv_nsec < 0)) {
          std::cerr << "ERROR: client activity time in the future" << std::endl;
          continue;
        }
        if (elapsed.tv_sec >= static_cast<time_t>(timeout_sec))
          // Client has timed out
          clients_to_disconnect.push_back(client_fd);
        else {
          // Calculate remaining time until this client times out
          const long remaining = timeout_sec - elapsed.tv_sec;
          if (epoll_timeout == -1 || remaining < epoll_timeout)
            epoll_timeout =
                remaining * 1000 -
                elapsed.tv_nsec / 1000000; // Convert to milliseconds
        }
      }
    }

    // Disconnect timed-out clients
    for (size_t i = 0; i < clients_to_disconnect.size(); ++i)
      disconnect(clients_to_disconnect[i]);

    // Clean expired sessions (use the first server's timeout as default)
    if (clients.begin() != clients.end()) {
      const int session_timeout =
          clients.begin()->second.config->get_server_response_time();
      if (session_timeout > 0)
        sessions.clean_expired_sessions(session_timeout);
    }

    // apply CGI timeout
    std::set<CgiDelegate *> cgis_to_reap;
    for (std::map<const FileDescriptor *,
                  std::pair<const FileDescriptor *, CgiDelegate *> >::iterator
             it = cgis.begin();
         it != cgis.end();) {
      CgiDelegate *cgi = it->second.second;
      if (cgi->check_timeout()) {
        std::ostringstream oss;
        const Response resp(
            DefaultError::default_err_response(Response::GATEWAY_TIMEOUT));
        oss << resp;
        std::map<FileDescriptor const *, ClientSession>::iterator jt =
            clients.find(it->second.first);
        if (jt != clients.end()) {
          if (!resp.keep_alive)
            jt->second.dropping = true;
          jt->second.out_buff = oss.str();
          client_write(jt->first);
        }
        cgis_to_reap.insert(cgi);
      } else {
        const size_t cgi_remaining =
            cgi->remaining_ns() / 1000000; // milliseconds
        if (epoll_timeout == -1 ||
            cgi_remaining < static_cast<size_t>(epoll_timeout))
          epoll_timeout = static_cast<long>(cgi_remaining);
        ++it;
      }
    }
    for (std::set<CgiDelegate *>::const_iterator it = cgis_to_reap.begin();
         it != cgis_to_reap.end(); ++it)
      reap_cgi(*it);

    // Waiting for events using epoll
    Result<Events> events_result = epoll.wait(static_cast<int>(epoll_timeout));
    if (!events_result.has_value()) {
      if (events_result.error() == Errors::interrupted)
        continue;
      std::cerr << "ERROR: " << events_result.error() << std::endl;
      break;
    }

    for (Events events = events_result.value(); !events.is_end(); ++events) {
      Result<const Event *> ev_result = *events;
      if (!ev_result.has_value())
        continue;

      const Event *event = ev_result.value();
      const FileDescriptor *fd = &event->fd;
      dprintf(2, "epoll event on fd=%d\n", fd->_fd);
      // 1. 서버 소켓(문지기)인 경우 (listeners map에 Key가 존재함)
      if (listeners.find(fd) != listeners.end()) {
        new_connection(fd);
      } else if (clients.find(fd) !=
                 clients.end()) { // 2. 이미 연결된 클라이언트 소켓인 경우
        if (event->in)
          client_read(fd);
        if (event->out)
          client_write(fd);
        if (event->err || event->hup || event->rdhup) {
          if (clients.find(fd) != clients.end())
            client_read(fd);
          disconnect(fd);
        }
      } else { // CGI
        std::map<FileDescriptor const *,
                 std::pair<FileDescriptor const *, CgiDelegate *> >::iterator
            it = cgis.find(fd);

        if (it != cgis.end()) {
          FileDescriptor const *client_fd = it->second.first;
          CgiDelegate *cgi = it->second.second;
          Result<Void> res = cgi->handle_event(event);
          std::ostringstream oss;
          Response resp;
          if (!res.has_value()) {
            if (res.error() == Errors::gateway_timeout)
              resp =
                  DefaultError::default_err_response(Response::GATEWAY_TIMEOUT);
            else // res.error() == Errors::bad_gateway
              resp = DefaultError::default_err_response(Response::BAD_GATEWAY);
          } else {
            Result<std::string> output = cgi->poll();
            if (output.has_value()) {
              const Result<Response> res_ =
                  Response::from_cgi_outbuff(output.value());
              std::ostringstream oss;
              Response resp;
              if (res_.has_value())
                resp = res_.value();
              else
                resp =
                    DefaultError::default_err_response(Response::BAD_GATEWAY);
            }
            oss << resp;
            if (!resp.keep_alive)
              clients.at(client_fd).dropping = true;
            clients.at(client_fd).out_buff = oss.str();
            client_write(client_fd);
            reap_cgi(cgi);
          }
        }
      }
    }
  }
  clients.clear();
  return OKV;
}

void Server::reap_cgi(CgiDelegate *cgi) {
  for (std::map<FileDescriptor const *,
                std::pair<FileDescriptor const *, CgiDelegate *> >::iterator
           it = cgis.begin();
       it != cgis.end();) {
    if (it->second.second == cgi)
      cgis.erase(it++);
    else
      ++it;
  }
  cgi->~CgiDelegate();
  operator delete(cgi);
}
