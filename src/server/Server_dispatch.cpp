#include "DefaultError.hpp"
#include "Response.hpp"
#include "ResponseHandlers.hpp"
#include "Server.hpp"

Result<Void> Server::dispatch_request(const FileDescriptor *client_fd,
                                      ClientSession &client, char **envp) {
  Result<RouteRule_CGI> cgi_path_res = client.config->find_route_cgi(
      client.req->get_method(), client.req->get_path());
  const RouteRule_CGI *cgi_path = NULL;
  if (cgi_path_res.has_value())
    cgi_path = &cgi_path_res.value();

  // Log request details
  Result<size_t> content_length = client.req->get_content_length();
  std::cout << "\n" << utils::info << "client ip: " << client.ip << std::endl;
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
    std::cout << utils::info << "[Request]" << it->first << ": " << it->second
              << std::endl;
  }

  // Dispatch to CGI if applicable
  if (cgi_path != NULL) {
    TRY_(Void, Void,
         ResponseHandlers::register_cgi(*client.req, *cgi_path, &epoll,
                                        config.get_global_cgi(), cgis,
                                        client_fd, envp))
    delete client.req;
    client.req = NULL;
    return OKV;
  }

  // Generate normal HTTP response
  Response http = ResponseHandlers::http_response(client.req, &client,
                                                  mime_type, &sessions, envp);
  http.print_simple(std::cout);

  Result<Void> qr = queue_response(client_fd, http);
  delete client.req;
  client.req = NULL;
  if (!qr.has_value())
    return ERR(Void, qr.error());

  return OKV;
}

Result<Void> Server::queue_response(const FileDescriptor *client_fd,
                                    const Response &response) {
  if (clients.find(client_fd) == clients.end())
    return OKV;
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
      client.out_file_path = response.file_path;
      client.out_file_offset = 0;
      if (!client.out_file_path.empty()) {
        if (client.req->get_method() == Request::GET)
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
  return OKV;
}

Result<Void> Server::reap_cgi(CgiDelegate *cgi) {
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
  return OKV;
}
