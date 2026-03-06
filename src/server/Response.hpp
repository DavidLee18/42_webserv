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
        std::string mime_type;
};

class Response
{
public:
        static HttpResponse generate(const Http::Request* request, const ServerConfig *config);
private:
        static int check_path_type(const std::string &path);
        static std::string resolve_full_path(const Http::Request* request, const ServerConfig *config);
        static std::string get_pwd();
        static std::string error_file_path(int error_code);
};

#endif