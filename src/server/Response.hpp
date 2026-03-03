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

struct HttpResponse
{
	std::string status_code;
	std::string body;
};

class PathRoute
{
public:
	static HttpResponse get_file_content(const std::string path);
};


#endif