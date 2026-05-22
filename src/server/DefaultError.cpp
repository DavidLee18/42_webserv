#include "DefaultError.hpp"

static std::string generate_error_page(const std::string &heading) {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta "
         "charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>" +
         heading +
         "</h1>\n<p> </p>\n<a href=\"/\">Back to "
         "main</a>\n<br>\n</body>\n</html>";
}

Response::StatusCode
DefaultError::int_to_status_code(unsigned short status_code) {
    switch (status_code) {
    case 200: return Response::OK;
    case 204: return Response::NO_CONTENT;
    case 301: return Response::MOVED_PERMANENTLY;
    case 302: return Response::FOUND;
    case 400: return Response::BAD_REQUEST;
    case 401: return Response::UNAUTHORIZED;
    case 403: return Response::FORBIDDEN;
    case 404: return Response::NOT_FOUND;
    case 405: return Response::METHOD_NOT_ALLOWED;
    case 408: return Response::REQUEST_TIMEOUT;
    case 409: return Response::CONFLICT;
    case 413: return Response::PAYLOAD_TOO_LARGE;
    case 501: return Response::NOT_IMPLEMENTED;
    case 502: return Response::BAD_GATEWAY;
    case 504: return Response::GATEWAY_TIMEOUT;
    default: return Response::INTERNAL_SERVER_ERR;
    }
}

std::string
DefaultError::status_code_to_string(const Response::StatusCode status_code) {
    switch (status_code) {
    case Response::OK: return "200 OK";
    case Response::NO_CONTENT: return "204 No Content";
    case Response::MOVED_PERMANENTLY: return "301 Moved Permanently";
    case Response::FOUND: return "302 Found";
    case Response::BAD_REQUEST: return "400 Bad Request";
    case Response::UNAUTHORIZED: return "401 Unauthorized";
    case Response::FORBIDDEN: return "403 Forbidden";
    case Response::NOT_FOUND: return "404 Not Found";
    case Response::METHOD_NOT_ALLOWED: return "405 Method Not Allowed";
    case Response::REQUEST_TIMEOUT: return "408 Request Timeout";
    case Response::CONFLICT: return "409 Conflict";
    case Response::PAYLOAD_TOO_LARGE: return "413 Payload Too Large";
    case Response::NOT_IMPLEMENTED: return "501 Not Implemented";
    case Response::BAD_GATEWAY: return "502 Bad Gateway";
    case Response::GATEWAY_TIMEOUT: return "504 Gateway Timeout";
    default: return "500 Internal Server Error";
    }
}

Response
DefaultError::default_err_response(const Response::StatusCode err_code) {
    Response response;

  response.version = "HTTP/1.1";
  response.content_type = "text/html";
  response.status_code = err_code;
  response.keep_alive = false;
  response.body =
      generate_error_page(status_code_to_string(err_code) + " Error");
  response.content_length = response.body.length();
  return response;
}
