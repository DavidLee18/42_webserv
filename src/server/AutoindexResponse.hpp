#ifndef AUTOINDEXRESPONSE_HPP
#define AUTOINDEXRESPONSE_HPP

#include "Response.hpp"
#include "ResponseUtils.hpp"
#include <dirent.h>

class AutoindexResponse {
public:
  static Result<std::string> generate(const std::string &real_path,
                                      const std::string &req_uri, DIR *dir);
};

#endif