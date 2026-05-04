#include "Response.hpp"

std::string DefaultError::bad_request() {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta "
         "charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>404 Error: Bad Request</h1>\n<p> "
         "</p>\n<a href=\"/\">Back to main</a>\n<br>\n</body>\n</html>";
}

std::string DefaultError::forbidden() {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta "
         "charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>404 Error: Forbidden</h1>\n<p> "
         "</p>\n<a href=\"/\">Back to main</a>\n<br>\n</body>\n</html>";
}

std::string DefaultError::not_found() {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta "
         "charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>404 Error: Not Found</h1>\n<p> "
         "</p>\n<a href=\"/\">Back to main</a>\n<br>\n</body>\n</html>";
}

std::string DefaultError::server_error() {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
         "<meta charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>500 Error: Internal Server "
         "Error</h1>\n<p> </p>\n<a href=\"/\">Back to "
         "main</a>\n<br>\n</body>\n</html>";
}

std::string DefaultError::unknown_err() {
  return "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
         "<meta charset=\"UTF-8\">\n<title>Webserv</title>\n<style>\nbody { "
         "font-family: Arial, sans-serif; text-align: center; margin-top: "
         "50px; }\na { display: inline-block; margin: 15px; padding: 10px "
         "20px; background-color: #007bff; color: white; text-decoration: "
         "none; border-radius: 5px; }\na:hover { background-color: #0056b3; "
         "}\n</style>\n</head>\n<body>\n<h1>UNKNOWN "
         "ERROR</h1>\n<p> </p>\n<a href=\"/\">Back to "
         "main</a>\n<br>\n</body>\n</html>";
}

std::string
DefaultError::status_code_to_string(const Response::StatusCode status_code) {
  switch (status_code) {
  case Response::OK:
    return "200 OK";
  case Response::MOVED_PERMANENTLY:
    return "301 Moved Permanently";
  case Response::BAD_REQUEST:
    return "400 Bad Request";
  case Response::FORBIDDEN:
    return "403 Forbidden";
  case Response::NOT_FOUND:
    return "404 Not Found";
  case Response::METHOD_NOT_ALLOWED:
    return "405 Method Not Allowed";
  case Response::PAYLOAD_TOO_LARGE:
    return "413 Payload Too Large";
  case Response::BAD_GATEWAY:
    return "502 Bad Gateway";
  case Response::GATEWAY_TIMEOUT:
    return "504 Gateway Timeout";
  default:
    return "500 Internal Server Error";
  }
}

Response
DefaultError::default_err_response(const Response::StatusCode err_code) {
  Response response;

  response.version = "HTTP/1.1";
  response.mime_type = "text/html";
  response.status_code = status_code_to_string(err_code);
  response.keep_alive = false;
  response.connection = "keep-alive";
  switch (err_code) {
  case Response::BAD_REQUEST:
    response.body = bad_request();
    break;
  case Response::FORBIDDEN:
    response.body = forbidden();
    break;
  case Response::NOT_FOUND:
    response.body = not_found();
    break;
  case Response::INTERNAL_SERVER_ERR:
    response.body = server_error();
    break;
  default:
    response.body = unknown_err();
    break;
  }
  return response;
}
