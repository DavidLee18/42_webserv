#include "DefaultError.hpp"
#include "Response.hpp"
#include "Server.hpp"
#include <vector>

Result<Void> Server::client_read(const FileDescriptor *client_fd, char **envp) {
  if (clients.find(client_fd) == clients.end()) {
    return OKV;
  }
  ClientSession &client = clients.at(client_fd);
  if (client.config == NULL) {
    Response resp(
        DefaultError::default_err_response(Response::INTERNAL_SERVER_ERR));
    resp.print_simple(std::cout);
    queue_response(client_fd, resp);
    return OKV;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
    std::cerr << utils::error << "failed to update client activity time"
              << std::endl;
    return ERR(Void, "failed to update client activity time");
  }
  while (true) { // repeat until nothing to read
    char buf[NETWORK_BUFFER_SIZE];
    Result<ssize_t> recv_res = client_fd->sock_recv(buf, sizeof(buf));
    if (!recv_res.has_value())
      break; // EWOULDBLOCK

    if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
      std::cerr << utils::error << "failed to update client activity time"
                << std::endl;
      return ERR(Void, "failed to update client activity time");
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
          return OKV;
        } else if (req_.error() == Errors::malformed_header ||
                   req_.error() == Errors::bad_request) {
          Response resp(
              DefaultError::default_err_response(Response::BAD_REQUEST));
          resp.headers = client.config->get_header();
          resp.print_simple(std::cout);
          {
            Result<Void> qr = queue_response(client_fd, resp);
            if (!qr.has_value())
              return ERR(Void, qr.error());
          }
          return OKV;
        } else if (req_.error() == Errors::not_implemented) {
          Response resp =
              DefaultError::default_err_response(Response::NOT_IMPLEMENTED);
          resp.headers = client.config->get_header();
          resp.print_simple(std::cout);
          queue_response(client_fd, resp);
          return OKV;
        } else {
          Response resp =
              DefaultError::default_err_response(Response::INTERNAL_SERVER_ERR);
          resp.headers = client.config->get_header();
          resp.print_simple(std::cout);
          queue_response(client_fd, resp);
          return OKV;
        }
      }
      client.req = req_.value();
      if (client.req->is_partial()) {
        if (client.dropping)
          disconnect(client_fd);
        return OKV;
      }

      Result<Void> dis_res = dispatch_request(client_fd, client, envp);
      if (!dis_res.has_value())
        std::cerr << dis_res.error() << std::endl;
      // Client may have been disconnected in dispatch_request, check existence
      if (clients.find(client_fd) == clients.end())
        return OKV;
      continue;
    } else {
      // Refresh client reference before continuing
      if (clients.find(client_fd) == clients.end())
        return OKV;
      const Result<Void> parse_more = client.req->continue_parsing(in_buffer);
      if (!parse_more.has_value()) {
        std::cerr << utils::error
                  << "request body parsing failed: " << parse_more.error()
                  << std::endl;
        Response resp(
            DefaultError::default_err_response(Response::BAD_REQUEST));
        resp.headers = client.config->get_header();
        resp.print_simple(std::cout);
        TRY_(Void, Void, queue_response(client_fd, resp))
        delete client.req;
        client.req = NULL;
        return OKV;
      }
      const Result<size_t> content_len = client.req->get_content_length();
      Result<RouteRule> rule_res = client.config->find_route(
          client.req->get_method(), client.req->get_path());
      if (!rule_res.has_value()) {
        Response resp(DefaultError::default_err_response(Response::NOT_FOUND));
        resp.headers = client.config->get_header();
        resp.print_simple(std::cout);
        TRY_(Void, Void, queue_response(client_fd, resp))
        delete client.req;
        client.req = NULL;
        return OKV;
      }
      const RouteRule rule = rule_res.value();
      if (content_len.has_value() &&
          (static_cast<size_t>(rule.max_body_KB) * 1024 < content_len.value() ||
           (client.req->is_partial() &&
            content_len.value() <= client.req->get_body().size()) ||
           (!client.req->is_partial() &&
            content_len.value() < client.req->get_body().size()))) {
        Response resp(
            DefaultError::default_err_response(Response::PAYLOAD_TOO_LARGE));
        resp.headers = client.config->get_header();
        resp.print_simple(std::cout);
        TRY_(Void, Void, queue_response(client_fd, resp))
        delete client.req;
        client.req = NULL;
        return OKV;
      }
      if (client.req->is_partial())
        return OKV;

      Result<Void> dis_res = dispatch_request(client_fd, client, envp);
      if (!dis_res.has_value())
        std::cerr << dis_res.error() << std::endl;
      // Client may have been disconnected in dispatch_request, check existence
      if (clients.find(client_fd) == clients.end())
        return OKV;
      continue;
    }
  }
  TRY_(Void, Void, client_write(client_fd))
  // EPOLLIN | EPOLLOUT
  return OKV;
}

Result<Void> Server::client_write(const FileDescriptor *client_fd) {
  if (clients.find(client_fd) == clients.end())
    return OKV;
  ClientSession &client = clients.at(client_fd);
  if (clock_gettime(CLOCK_MONOTONIC, &client.last_activity_time) != 0) {
    std::cerr << utils::error << "failed to update client activity time"
              << std::endl;
    return ERR(Void, "failed to update client activity time");
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
        return OKV;
      }
    }
  } else if (client.dropping) {
    disconnect(client_fd);
    return OKV;
  }

  // Stream file in chunks: keep reading and queuing until EOF or buffer fills
  std::ifstream infile(client.out_file_path.c_str(), std::ios::binary);
  if (!infile.is_open()) {
    if (client.dropping)
      disconnect(client_fd);
    return OKV;
  }
  infile.seekg(static_cast<std::streamoff>(client.out_file_offset));
  if (infile.fail()) {
    std::cerr << utils::error << "Stream error: Failed to seek to offset "
              << client.out_file_offset << " for file " << client.out_file_path
              << std::endl;
    client.streaming_file = false;
    client.out_file_path.clear();
    if (client.dropping && client.out_buff.empty()) {
      disconnect(client_fd);
    }
    return OKV;
  }
  while (client.streaming_file && write_buffer.empty()) {
    const size_t CHUNK = NETWORK_BUFFER_SIZE; // 4096
    std::vector<char> buf(CHUNK);
    infile.read(buf.data(), static_cast<std::streamsize>(CHUNK));
    std::streamsize read_bytes = infile.gcount();
    if (infile.fail() && !infile.eof()) {
      std::cerr << utils::error << "Stream error: I/O read failure on "
                << client.out_file_path << std::endl;
      client.streaming_file = false;
      client.out_file_path.clear();
      if (client.dropping && client.out_buff.empty()) {
        disconnect(client_fd);
      }
      return OKV;
    }
    if (read_bytes > 0) {
      client.out_file_offset += static_cast<size_t>(read_bytes);
      client.out_buff.append(buf.data(), static_cast<size_t>(read_bytes));
      // Try sending what we just read
      while (!client.out_buff.empty()) {
        Result<ssize_t> send_res = client_fd->sock_send(
            client.out_buff.c_str(), client.out_buff.length());
        if (!send_res.has_value()) {
          std::cout << utils::info << "stream: send would block, remaining="
                    << client.out_buff.size() << std::endl;
          break;
        }
        const ssize_t bytes = send_res.value();
        if (bytes == 0)
          break;
        client.out_buff.erase(0, static_cast<std::size_t>(bytes));
      }
    }

    if (infile.eof()) {
      client.streaming_file = false;
      client.out_file_path.clear();
      if (client.dropping && client.out_buff.empty())
        disconnect(client_fd);
      break;
    }
    // If buffer is not empty after send, break to wait for next EPOLLOUT event
    if (!write_buffer.empty())
      break;
  }
  return OKV;
}
