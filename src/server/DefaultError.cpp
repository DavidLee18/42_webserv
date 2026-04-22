#include "DefaultError.hpp"
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

std::string DefaultError::status_code_to_string(int status_code) {
  if (status_code == 200)
    return "200 OK";
  else if (status_code == 301)
    return "301 Moved Permanently";
  else if (status_code == 400)
    return "400 Bad Request";
  else if (status_code == 403)
    return "403 Forbidden";
  else if (status_code == 404)
    return "404 Not Found";
  else if (status_code == 405)
    return "405 Method Not Allowed";
  else if (status_code == 413)
    return "413 Payload Too Large";
  else if (status_code == 500)
    return "500 Internal Server Error";
  return "500 Internal Server Error";
}

Response DefaultError::default_err_response(int err_code) {
  Response response;

  response.version = "HTTP/1.1";
  response.mime_type = "text/html";
  response.status_code = status_code_to_string(err_code);
  if (err_code == BAD_REQUEST)
    response.body = bad_request();
  else if (err_code == FORBIDDEN_ERR)
    response.body = forbidden();
  else if (err_code == NOT_FOUND_ERR)
    response.body = not_found();
  else if (err_code == INTERNAL_SERVER_ERR)
    response.body = server_error();
  else
    response.body = unknown_err();
  response.mime_type = "text/html";
  response.status_code = status_code_to_string(err_code);
  return response;
}
