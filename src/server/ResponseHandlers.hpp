#ifndef RESPONSE_HANDLERS_HPP
#define RESPONSE_HANDLERS_HPP

#include "../config/ServerConfig.hpp"
#include "../core/Errors.hpp"
#include "Response.hpp"

class EPoll;
class CgiDelegate;
struct Target;

class ResponseHandlers {
public:
  static Response
  http_response(const Request *request, const ClientSession *client,
                const std::map<std::string, std::string> &mime_type,
                Session *session, char **envp);

  static Result<Void> register_cgi(
      const Request &request, const RouteRule_CGI &rule, EPoll *epoll,
      std::map<std::string, std::string> const &cgi_interpreters,
      std::map<FileDescriptor const *,
               std::pair<FileDescriptor const *, CgiDelegate *> > &cgis,
      FileDescriptor const *client_fd, char **envp);

  static Response delete_method_response(const Target &target,
                                         Response response,
                                         const ServerConfig *config,
                                         const RouteRule *rule, char **envp);
  static Response post_method_response(const Target &target, Response response,
                                       const ClientSession *client,
                                       const RouteRule *rule,
                                       const Request *request, Session *session,
                                       char **envp);
  static Response get_method_response(Target target, Response response,
                                      const ServerConfig *config,
                                      const RouteRule *rule,
                                      const Request *request, char **envp);
};

#endif
