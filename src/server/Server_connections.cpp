#include "DefaultError.hpp"
#include "Response.hpp"
#include "ResponseHandlers.hpp"
#include "Server.hpp"
#include <ctime>

Result<Void> Server::new_connection(const FileDescriptor *server_fd) {
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
        return ERR(Void, std::string("accept failed: ") + err);
      }
    }
    FileDescriptor client_fd = client_result.value();
    TRY_(Void, Void, client_fd.set_nonblocking())
    TRY_(Void, Void, client_fd.close_on_exec())

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

    FileDescriptor *client_ptr;
    TRY(Void, FileDescriptor *, client_ptr,
        epoll.add_fd(client_fd, client_event, client_option))
    if (listeners.find(server_fd) != listeners.end()) {
      client.config = listeners.at(server_fd);
    }
    clients[client_ptr] = client;
    std::cout << utils::info << "New client connected!" << std::endl;
  }
  return OKV;
}

Result<Void> Server::disconnect(const FileDescriptor *client_fd) {
  epoll.del_fd(client_fd);
  clients.erase(client_fd);
  return OKV;
}
