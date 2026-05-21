#ifndef RESPONSE_ERRORS_HPP
#define RESPONSE_ERRORS_HPP

#include "../config/ServerConfig.hpp"
#include "Response.hpp"

struct Target;

class ResponseErrors {
public:
  static Response error_response(const ServerConfig *config,
                                 const RouteRule *rule,
                                 Response::StatusCode error_code, char **envp);
  static Target resolve_target(const RouteRule *rule,
                               const ServerConfig *config,
                               const Request *request, char **envp);
};

#endif
