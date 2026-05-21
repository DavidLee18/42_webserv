#include "Server.hpp"
#include "Response.hpp"
#include "DefaultError.hpp"
#include <cstddef>
#include <ctime>
#include <vector>

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
  while (g_receivedSignal == 0) {
    long epoll_timeout = -1; // Default: wait indefinitely

    // Check for client timeouts and calculate epoll timeout
    timespec now = {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      std::cerr << utils::error
                << "failed to get current time for client timeout check"
                << std::endl;
      continue;
    }

    // Collect clients to disconnect or flush (avoid modifying map during
    // iteration)
    std::vector<const FileDescriptor *> clients_to_disconnect;
    std::vector<std::pair<const FileDescriptor *, Response> > clients_to_flush;

    for (std::map<const FileDescriptor *, ClientSession>::iterator it =
             clients.begin();
         it != clients.end(); ++it) {
      const FileDescriptor *client_fd = it->first;
      ClientSession &session = it->second;

      if (session.config == NULL)
        continue;

      const long timeout_ms =
          static_cast<long>(session.config->get_server_response_time());
      if (timeout_ms > 0) {
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

        if (session.req == NULL && session.in_buff.empty() &&
            elapsed_ms >= idle_timeout_ms) {
          clients_to_disconnect.push_back(client_fd);
        } else if (elapsed_ms >= timeout_ms &&
                   (session.req == NULL ||
                    (!session.req->is_partial() && session.in_buff.empty()))) {
          clients_to_disconnect.push_back(client_fd);
        } else if (elapsed_ms >= chunked_pending_ms &&
                   (session.req &&
                    (session.req->is_partial() || !session.in_buff.empty()))) {
          Response resp =
              DefaultError::default_err_response(Response::REQUEST_TIMEOUT);
          clients_to_flush.push_back(std::make_pair(client_fd, resp));
        } else {
          long remaining_ms;
          if (session.req == NULL && session.in_buff.empty())
            remaining_ms = static_cast<long>(idle_timeout_ms - elapsed_ms);
          else
            remaining_ms = static_cast<long>(timeout_ms - elapsed_ms);
          if (epoll_timeout == -1 || remaining_ms < epoll_timeout)
            epoll_timeout = remaining_ms;
        }
      }
    }

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
      {
        Result<Void> cw = client_write(jt->first);
        if (!cw.has_value())
          return ERR(Void, cw.error());
      }
      if (clients.find(fd) != clients.end() && jt->second.dropping) {
        Result<Void> d = disconnect(fd);
        if (!d.has_value())
          return ERR(Void, d.error());
      }
    }

    for (size_t i = 0; i < clients_to_disconnect.size(); ++i) {
      Result<Void> d = disconnect(clients_to_disconnect[i]);
      if (!d.has_value())
        return ERR(Void, d.error());
    }

    if (clients.begin() != clients.end()) {
      const unsigned int session_timeout_ms =
          clients.begin()->second.config->get_server_response_time();
      if (session_timeout_ms > 0)
        sessions.clean_expired_sessions(
            static_cast<int>(session_timeout_ms / 1000));
    }

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
          {
            Result<Void> cw = client_write(jt->first);
            if (!cw.has_value())
              return ERR(Void, cw.error());
          }
        }
        cgis_to_reap.insert(cgi);
      } else if (it->second.second->wait_or_reap()) {
        const Result<std::string> resp_res = it->second.second->poll();
        Response resp;
        if (resp_res.has_value()) {
          std::map<FileDescriptor const *, ClientSession>::iterator jt =
              clients.find(it->second.first);
          if (jt == clients.end()) {
            cgis_to_reap.insert(cgi);
            continue;
          }
          const Result<Response> resp_res2 = Response::from_cgi_outbuff(
              resp_res.value(), jt->second.config->get_header());
          resp =
              resp_res2.has_value()
                  ? resp_res2.value()
                  : DefaultError::default_err_response(Response::BAD_GATEWAY);
        } else {
          resp = DefaultError::default_err_response(Response::BAD_GATEWAY);
        }

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
            cgi->remaining_ns() / static_cast<size_t>(1e6);
        if (epoll_timeout == -1 ||
            cgi_remaining < static_cast<size_t>(epoll_timeout))
          epoll_timeout = static_cast<long>(cgi_remaining);
      }
    }
    for (std::set<CgiDelegate *>::const_iterator it = cgis_to_reap.begin(); it != cgis_to_reap.end(); ++it) {
      Result<Void> rr = reap_cgi(*it);
      if (!rr.has_value())
        return ERR(Void, rr.error());
    }

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
      if (listeners.find(fd) != listeners.end()) {
        {
          Result<Void> nc = new_connection(fd);
          if (!nc.has_value())
            return ERR(Void, nc.error());
        }
      } else if (clients.find(fd) != clients.end()) {
        if (event->in) {
          Result<Void> cr = client_read(fd, envp);
          if (!cr.has_value())
            return ERR(Void, cr.error());
        }
        if (event->out) {
          Result<Void> cw = client_write(fd);
          if (!cw.has_value())
            return ERR(Void, cw.error());
        }
        if (event->err || event->hup || event->rdhup) {
          if (clients.find(fd) != clients.end()) {
            Result<Void> cr = client_read(fd, envp);
            if (!cr.has_value())
              return ERR(Void, cr.error());
          }
          Result<Void> d = disconnect(fd);
          if (!d.has_value())
            return ERR(Void, d.error());
        }
      } else {
        std::map<FileDescriptor const *,
                 std::pair<const FileDescriptor *, CgiDelegate *> >::iterator
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
            else
              resp = DefaultError::default_err_response(Response::BAD_GATEWAY);
          } else {
            Result<std::string> output = cgi->poll();
            std::map<FileDescriptor const *, ClientSession>::iterator jt =
                clients.find(client_fd);
            if (jt == clients.end()) {
              Result<Void> rr = reap_cgi(cgi);
              if (!rr.has_value())
                return ERR(Void, rr.error());
              continue;
            }
            if (output.has_value()) {
              const Result<Response> res_ = Response::from_cgi_outbuff(
                  output.value(), jt->second.config->get_header());
              if (res_.has_value())
                resp = res_.value();
              else
                resp =
                    DefaultError::default_err_response(Response::BAD_GATEWAY);
            } else {
              continue;
            }
            resp.print_simple(std::cout);
            oss << resp;
            if (!resp.keep_alive)
              jt->second.dropping = true;
            jt->second.out_buff = oss.str();
            {
              Result<Void> cw = client_write(client_fd);
              if (!cw.has_value())
                return ERR(Void, cw.error());
            }
            {
              Result<Void> rr = reap_cgi(cgi);
              if (!rr.has_value())
                return ERR(Void, rr.error());
            }
          }
        }
      }
    }
  }
  clients.clear();
  return OKV;
}


