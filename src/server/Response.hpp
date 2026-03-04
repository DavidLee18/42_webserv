#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#define NOT_FOUND		0
#define IS_DIRECTORY	1
#define IS_FILE			2
#define FORBIDDEN		3

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "../ServerConfig.hpp"

struct StatusInfo
{
    std::string message;
    std::string file_path;
};

struct HttpResponse
{
	std::string status_code;
	std::string body;
};

class ResponseGenerator
{
public:
        static HttpResponse generate(const Http::Request* request, const ServerConfig *config);
private:
        static int check_path_type(const std::string &path);
        static std::string resolve_full_path(const Http::Request* request, const ServerConfig *config);
        static HttpResponse make_error_response(int error_code, const ServerConfig *config);
};

#endif