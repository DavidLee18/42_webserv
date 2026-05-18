#include "Server.hpp"
#include "../webserv.h"
#include "Response.hpp"
#include <cstddef>
#include <ctime>
#include <vector>

void Server::dispatch_request(const FileDescriptor *client_fd,
                              ClientSession &client, char **envp) {
  RouteRule_CGI const *cgi_path = client.config->find_route_cgi(
      client.req->get_method(), client.req->get_path());

  // Log request details
  Result<size_t> content_length = client.req->get_content_length();
  std::cout << "\n"
            << utils::info << "client ip: " << client.ip << std::endl;
  std::cout << utils::info << "[Request] " << client.req->get_method_string()
            << " " << client.req->get_path() << " (Body: ";
  if (content_length.has_value())
    std::cout << content_length.value();
  else
    std::cout << "(non-existent)";
  std::cout << " bytes)" << std::endl;
  for (std::map<std::string, std::string>::const_iterator it =
           client.req->get_headers().begin();
       it != client.req->get_headers().end(); ++it) {
    std::cout << utils::info << "[Request]" << it->first << ": "
              << it->second << std::endl;
  }

  // Dispatch to CGI if applicable
  if (cgi_path != NULL) {
    Result<Void> del_ = ServerResponse::register_cgi(
        *client.req, *cgi_path, &epoll, config.get_global_cgi(), cgis,
        client_fd, envp);
    if (!del_.has_value())
      std::cerr << utils::error << "CGI registration failed: " << del_.error()
                << std::endl;
    delete client.req;
    client.req = NULL;
    return;
  }

  // Generate normal HTTP response
  Response http = ServerResponse::http_response(client.req, &client,
                                                mime_type, &sessions, envp);
  http.print_simple(std::cout);

  delete client.req;
  client.req = NULL;

  queue_response(client_fd, http);
}

void Server::queue_response(const FileDescriptor *client_fd,
                            const Response &response) {
  if (clients.find(client_fd) == clients.end())
    return;
  ClientSession &client = clients.at(client_fd);

  const size_t STREAM_THRESHOLD = 64 * 1024; // 64 KiB

  if (!response.file_path.empty()) {
    // For small files, buffer entirely; for large files, stream from disk.
    if (response.content_length <= STREAM_THRESHOLD) {
      std::ifstream infile(response.file_path.c_str(), std::ios::binary);
      if (infile.is_open()) {
        std::ostringstream ss;
        ss << response; // headers
        ss << infile.rdbuf();
        client.out_buff += ss.str();
        infile.close();
      } else {
        // Fallback: send 404-like error
        Response err = DefaultError::default_err_response(Response::NOT_FOUND);
        std::ostringstream ss;
        ss << err;
        client.out_buff += ss.str();
        client.dropping = true;
      }
    } else {
      // Stream: send headers now, keep file open to stream body later
      std::ostringstream ss;
      ss << response; // headers only (body is not set)
      client.out_buff += ss.str();
      client.out_file.open(response.file_path.c_str(), std::ios::binary);
      if (client.out_file.is_open()) {
        client.out_file_offset = 0;
        client.streaming_file = true;
      } else {
        // Could not open: send 404
        Response err = DefaultError::default_err_response(Response::NOT_FOUND);
        std::ostringstream es;
        es << err;
        client.out_buff += es.str();
        client.dropping = true;
      }
    }
  } else {
    std::ostringstream oss;
    oss << response;
    client.out_buff += oss.str();
  }

  // Mark for disconnection if client doesn't want keep-alive
  if (!response.keep_alive)
    client.dropping = true;
}

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
        std::cerr << utils::error << "accept failed: " << err << std::endl;
        break;
      }
    }
    FileDescriptor client_fd = client_result.value();
    if (!client_fd.set_nonblocking().has_value()) {
      std::cerr << utils::error
                << "failed to set client socket to non-blocking mode"
                << std::endl;
      continue;
    }

    if (!client_fd.close_on_exec().has_value()) {
      std::cerr << utils::error
                << "failed to set client socket to close-on-exec mode"
                << std::endl;
      continue;
    }

    ClientSession client;
    if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
      std::cerr << utils::error
                << "failed to get current time for client activity"
                << std::endl;
      continue;
    }
    std::ostringstream oss;
    const unsigned char *octets =
        reinterpret_cast<unsigned char *>(&client_addr.sin_addr.s_addr);
    oss << static_cast<unsigned int>(octets[0]) << '.'
        << static_cast<unsigned int>(octets[1]) << '.'
        << static_cast<unsigned int>(octets[2]) << '.'
        << static_cast<unsigned int>(octets[3]);
    client.ip = oss.str();

    // register client socket to EPoll
    Event client_event(NULL, true, true, true, false, false, false);
    Option client_option(true, false, false, false);

    Result<FileDescriptor *> add_result =
        epoll.add_fd(client_fd, client_event, client_option);
    if (add_result.has_value()) {
      const FileDescriptor *client_ptr = add_result.value();
      if (listeners.find(server_fd) != listeners.end()) {
        client.config = listeners.at(server_fd);
      }
      clients[client_ptr] = client;
      std::cout << utils::info << "New client connected!" << std::endl;
    } else {
      std::cerr << utils::error << "epoll add failed: " << add_result.error()
                << std::endl;
    }
  }
}

void Server::disconnect(const FileDescriptor *client_fd) {
  epoll.del_fd(client_fd);
  clients.erase(client_fd);
}

void Server::client_read(const FileDescriptor *client_fd, char **envp) {
  if (clients.find(client_fd) == clients.end()) {
    return;
  }
  ClientSession &client = clients.at(client_fd);
  if (client.config == NULL) {
    Response resp(
        DefaultError::default_err_response(Response::INTERNAL_SERVER_ERR));
    resp.print_simple(std::cout);
    queue_response(client_fd, resp);
    return;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
    std::cerr << utils::error << "failed to update client activity time"
              << std::endl;
    return;
  }
  while (true) { // repeat until nothing to read
    char buf[NETWORK_BUFFER_SIZE];
    Result<ssize_t> recv_res = client_fd->sock_recv(buf, sizeof(buf));
    if (!recv_res.has_value())
      break; // EWOULDBLOCK

    if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
      std::cerr << utils::error << "failed to update client activity time"
                << std::endl;
      return;
    }
    ssize_t bytes = recv_res.value();
    if (bytes == 0) { // (EOF)
      client.dropping = true;
      break;
    }
    client.in_buff.append(buf, static_cast<std::size_t>(bytes));
  }

  // HTTP parsing and response generate
  std::string &in_buffer = client.in_buff;
  while (!in_buffer.empty()) {
    if (client.req == NULL) {
      Result<Request *> req_ = Request::from_buff(in_buffer);

      if (!req_.has_value()) {
        std::cerr << utils::error << "request parsing failed: " << req_.error()
                  << std::endl;
        if (req_.error() == Errors::incomplete_header) {
          if (client.dropping)
            disconnect(client_fd);
          return;
        } else if (req_.error() == Errors::malformed_header ||
                   req_.error() == Errors::bad_request) {
          Response resp(
              DefaultError::default_err_response(Response::BAD_REQUEST));
          resp.headers = client.config->get_header();
          resp.print_simple(std::cout);
          queue_response(client_fd, resp);
          return;
        } else if (req_.error() == Errors::not_implemented) {
          Response resp =
              DefaultError::default_err_response(Response::NOT_IMPLEMENTED);
          resp.headers = client.config->get_header();
          resp.print_simple(std::cout);
          queue_response(client_fd, resp);
          return;
        }
      }

      client.req = req_.value();
      const Result<size_t> req_cl = client.req->get_content_length();
      const RouteRule *rule = client.config->find_route(
          client.req->get_method(), client.req->get_path());
      if (rule != NULL && req_cl.has_value() &&
          req_cl.value() > static_cast<size_t>(rule->max_body_KB) * 1024) {
        Response resp(
            DefaultError::default_err_response(Response::PAYLOAD_TOO_LARGE));
        resp.headers = client.config->get_header();
        resp.print_simple(std::cout);
        queue_response(client_fd, resp);
        return;
      }
      if (client.req->is_partial()) {
        if (client.dropping)
          disconnect(client_fd);
        return;
      }

      dispatch_request(client_fd, client, envp);
      // Client may have been disconnected in dispatch_request, check existence
      if (clients.find(client_fd) == clients.end())
        return;
      continue;
    } else {
      // Refresh client reference before continuing
      if (clients.find(client_fd) == clients.end())
        return;
      client.req->continue_parsing(in_buffer);
      const Result<size_t> content_len = client.req->get_content_length();
      const RouteRule *const rule = client.config->find_route(
          client.req->get_method(), client.req->get_path());
      if (content_len.has_value() && rule != NULL &&
          (static_cast<size_t>(rule->max_body_KB) * 1024 <
               content_len.value() ||
           (client.req->is_partial() &&
            content_len.value() <= client.req->get_body().size()) ||
           (!client.req->is_partial() &&
            content_len.value() < client.req->get_body().size()))) {
        Response resp(
            DefaultError::default_err_response(Response::PAYLOAD_TOO_LARGE));
        resp.headers = client.config->get_header();
        resp.print_simple(std::cout);
        queue_response(client_fd, resp);
        delete client.req;
        client.req = NULL;
        return;
      }
      if (client.req->is_partial())
        return;

      dispatch_request(client_fd, client, envp);
      // Client may have been disconnected in dispatch_request, check existence
      if (clients.find(client_fd) == clients.end())
        return;
      continue;
    }
  }
  client_write(client_fd); // when the response is generated freshly, likely
                           // EPOLLIN | EPOLLOUT
}

void Server::client_write(const FileDescriptor *client_fd) {
  if (clients.find(client_fd) == clients.end())
    return;
  ClientSession &client = clients.at(client_fd);
  if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
    std::cerr << utils::error << "failed to update client activity time"
              << std::endl;
    return;
  }
  std::string &write_buffer = client.out_buff;
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
        if (client.dropping)
          disconnect(client_fd);
        return;
      }
    }
  } else if (client.dropping)
    disconnect(client_fd);

  // If we've emptied the write buffer and are streaming a file, load next
  // chunk from disk into the buffer and attempt to send it immediately.
  if (write_buffer.empty() && client.streaming_file) {
    const size_t CHUNK = NETWORK_BUFFER_SIZE; // 4096
    std::vector<char> buf(CHUNK);
    if (!client.out_file.is_open()) {
      client.streaming_file = false;
    } else {
      client.out_file.seekg(static_cast<std::streamoff>(client.out_file_offset));
      client.out_file.read(buf.data(), static_cast<std::streamsize>(CHUNK));
      std::streamsize read_bytes = client.out_file.gcount();
      if (read_bytes > 0) {
        client.out_file_offset += static_cast<size_t>(read_bytes);
        client.out_buff.append(buf.data(), static_cast<size_t>(read_bytes));
        // Try sending what we just read
        while (!client.out_buff.empty()) {
          Result<ssize_t> send_res = client_fd->sock_send(
              client.out_buff.c_str(), client.out_buff.length());
          if (!send_res.has_value())
            break;
          const ssize_t bytes = send_res.value();
          if (bytes == 0)
            break;
          client.out_buff.erase(0, static_cast<std::size_t>(bytes));
        }
      }

      if (client.out_file.eof()) {
        client.out_file.close();
        client.streaming_file = false;
        if (client.dropping && client.out_buff.empty())
          disconnect(client_fd);
      }
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
    Event event(NULL, true, false, false, false, false, false); // in=true
    Option op(true, false, false, false);                       // et=true

    // Add server socket to EPoll
    Result<FileDescriptor *> add_result = epoll.add_fd(server_fd, event, op);
    if (!add_result.has_value())
      return ERR(Void, "Server register fail: " + add_result.error());

    // Save pointer to distinguish server sockets from client sockets
    FileDescriptor *fd_ptr = add_result.value();
    listeners[fd_ptr] = &it->second;
    const unsigned char *octets =
        reinterpret_cast<const unsigned char *>(&addr);
    std::cout << utils::info << "Server listening "
              << static_cast<unsigned int>(octets[0]) << '.'
              << static_cast<unsigned int>(octets[1]) << '.'
              << static_cast<unsigned int>(octets[2]) << '.'
              << static_cast<unsigned int>(octets[3]) << " : " << port
              << std::endl;
  }
  return OK(Void, Void());
}

Result<Void> Server::start(char **envp) {
  std::cout << utils::info << "Starting server loop..." << std::endl;
  long epoll_timeout = -1; // Default: wait indefinitely
  while (g_receivedSignal == 0) {
    // Check for client timeouts and calculate epoll timeout
    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      std::cerr << utils::error
                << "failed to get current time for client timeout check"
                << std::endl;
      continue;
    }

    // Collect clients to disconnect or flush (avoid modifying map during iteration)
    std::vector<const FileDescriptor *> clients_to_disconnect;
    std::vector<std::pair<const FileDescriptor *, Response> > clients_to_flush;

    for (std::map<const FileDescriptor *, ClientSession>::iterator it =
             clients.begin();
         it != clients.end(); ++it) {
      const FileDescriptor *client_fd = it->first;
      ClientSession &session = it->second;

      if (session.config == NULL)
        continue;

      // Server config provides timeout in milliseconds
      const long timeout_ms = static_cast<long>(
          session.config->get_server_response_time()); // ms
      if (timeout_ms > 0) {
        // compute elapsed in milliseconds safely
        const long long elapsed_ms =
            (now.tv_sec - session.last_activity_time.tv_sec) * 1000LL +
            (now.tv_nsec - session.last_activity_time.tv_nsec) / 1000000LL;
        if (elapsed_ms < 0) {
          std::cerr << utils::error << "client activity time in the future"
                    << std::endl;
          continue;
        }

        const long chunked_pending_ms = CHUNKED_PENDING_TIMEOUT * 1000L;
        const long idle_timeout_ms = IDLE_TIMEOUT * 1000L;

        // Check for idle connections (no request, no buffered input)
        if (session.req == NULL && session.in_buff.empty() &&
            elapsed_ms >= idle_timeout_ms) {
          // Idle client connection exceeded timeout -> schedule disconnect
          clients_to_disconnect.push_back(client_fd);
        } else if (elapsed_ms >= timeout_ms &&
            (session.req == NULL ||
             (!session.req->is_partial() && session.in_buff.empty()))) {
          // Client has timed out -> schedule disconnect
          clients_to_disconnect.push_back(client_fd);
        } else if (elapsed_ms >= chunked_pending_ms &&
                   (session.req &&
                    (session.req->is_partial() || !session.in_buff.empty()))) {
          // Pending chunked/partial read exceeded; schedule a 408 flush
          Response resp =
              DefaultError::default_err_response(Response::REQUEST_TIMEOUT);
          clients_to_flush.push_back(std::make_pair(client_fd, resp));
        } else {
          // Calculate remaining time until this client times out (ms)
          long remaining_ms;
          if (session.req == NULL && session.in_buff.empty()) {
            // Idle connection: use idle timeout
            remaining_ms = static_cast<long>(idle_timeout_ms - elapsed_ms);
          } else {
            // Active/partial request: use server response timeout
            remaining_ms = static_cast<long>(timeout_ms - elapsed_ms);
          }
          if (epoll_timeout == -1 || remaining_ms < epoll_timeout)
            epoll_timeout = remaining_ms;
        }
      }
    }

    // Apply pending flushes (safe to mutate clients now)
    for (size_t i = 0; i < clients_to_flush.size(); ++i) {
      const FileDescriptor *fd = clients_to_flush[i].first;
      const Response &resp = clients_to_flush[i].second;
      std::map<FileDescriptor const *, ClientSession>::iterator jt =
          clients.find(fd);
      if (jt == clients.end())
        continue;
      std::ostringstream oss;
      oss << resp;
      if (!resp.keep_alive)
        jt->second.dropping = true;
      jt->second.out_buff = oss.str();
      client_write(jt->first);
    }

    // Disconnect timed-out clients
    for (size_t i = 0; i < clients_to_disconnect.size(); ++i)
      disconnect(clients_to_disconnect[i]);

    // Clean expired sessions (use the first server's timeout as default)
    if (clients.begin() != clients.end()) {
      const unsigned int session_timeout_ms =
          clients.begin()->second.config->get_server_response_time();
      if (session_timeout_ms > 0)
        sessions.clean_expired_sessions(
            static_cast<int>(session_timeout_ms / 1000));
    }

    // apply CGI timeout
    std::set<CgiDelegate *> cgis_to_reap;
    for (std::map<const FileDescriptor *,
                  std::pair<const FileDescriptor *, CgiDelegate *> >::iterator
             it = cgis.begin();
         it != cgis.end(); ++it) {
      CgiDelegate *cgi = it->second.second;
      if (cgi->check_timeout()) {
        std::ostringstream oss;
        const Response resp(
            DefaultError::default_err_response(Response::GATEWAY_TIMEOUT));
        resp.print_simple(std::cout);
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
      } else if (it->second.second->wait_or_reap()) {
        const Result<std::string> resp_res = it->second.second->poll();
        Response resp;
        if (resp_res.has_value()) {
          const Result<Response> resp_res2 = Response::from_cgi_outbuff(
              resp_res.value(), std::map<std::string, std::string>());
          resp =
              resp_res2.has_value()
                  ? resp_res2.value()
                  : DefaultError::default_err_response(Response::BAD_GATEWAY);
        } else
          resp = DefaultError::default_err_response(Response::BAD_GATEWAY);

        resp.print_simple(std::cout);
        std::ostringstream oss;
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
            cgi->remaining_ns() / static_cast<size_t>(1e6); // milliseconds
        if (epoll_timeout == -1 ||
            cgi_remaining < static_cast<size_t>(epoll_timeout))
          epoll_timeout = static_cast<long>(cgi_remaining);
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
      std::cerr << utils::error << events_result.error() << std::endl;
      break;
    }

    for (Events events = events_result.value(); !events.is_end(); ++events) {
      Result<const Event *> ev_result = *events;
      if (!ev_result.has_value())
        continue;

      const Event *event = ev_result.value();
      const FileDescriptor *fd = event->fd;
      std::cerr << utils::debug << "epoll event on fd=" << fd->_fd << std::endl;
      // 1. 서버 소켓(문지기)인 경우 (listeners map에 Key가 존재함)
      if (listeners.find(fd) != listeners.end()) {
        new_connection(fd);
      } else if (clients.find(fd) !=
                 clients.end()) { // 2. 이미 연결된 클라이언트 소켓인 경우
        if (event->in)
          client_read(fd, envp);
        if (event->out)
          client_write(fd);
        if (event->err || event->hup || event->rdhup) {
          if (clients.find(fd) != clients.end())
            client_read(fd, envp);
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
              const Result<Response> res_ = Response::from_cgi_outbuff(
                  output.value(), clients.at(client_fd).config->get_header());
              if (res_.has_value())
                resp = res_.value();
              else
                resp =
                    DefaultError::default_err_response(Response::BAD_GATEWAY);
            } else
              continue;
            resp.print_simple(std::cout);
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
