#include "Response.hpp"
#include "../cgi_1_1.h"

std::string get_pwd() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != NULL) {
        return std::string(buffer);
    }
    return "";
}

std::string error_file_path(int error_code)
{
	if (error_code == 400)
		return get_pwd() + "/spool/www/error/400.html";
	else if (error_code == 403)
		return get_pwd() + "/spool/www/error/403.html";
	else if (error_code == 404)
		return get_pwd() + "/spool/www/error/404.html";
	return get_pwd() + "/spool/www/error/500.html";
}

int ResponseGenerator::check_path_type(const std::string &path)
{
	struct stat info;

	if (stat(path.c_str(), &info) != 0)
		return NOT_FOUND;
	else if (access(path.c_str(), R_OK) != 0)
		return FORBIDDEN;
	else if (info.st_mode & S_IFDIR)
		return IS_DIRECTORY;
	else if (info.st_mode & S_IFREG)
		return IS_FILE;
	return -1;
}

HttpResponse ResponseGenerator::generate(const Http::Request* request, const ServerConfig *config)
{
	HttpResponse response;
	std::string full_path = resolve_full_path(request, config);
	int path_type = check_path_type(full_path);

	if (path_type == IS_DIRECTORY)
	{
		std::string index_path = full_path + "/index.html";
		if (check_path_type(index_path) == IS_FILE)
		{
			full_path = index_path;
			path_type = IS_FILE;
		}
		else
			full_path = error_file_path(403);
	}

	if (path_type == FORBIDDEN)
		full_path = error_file_path(403);
	else if (path_type == NOT_FOUND)
		full_path = error_file_path(404);
	else if (path_type == -1)
		full_path = error_file_path(500);
	std::ifstream file(full_path.c_str());
	if (file.is_open())
	{
		std::ostringstream ss;
		ss << file.rdbuf();
		response.body = ss.str();
		response.status_code = "200 OK";
		file.close();
	}
	return response;

}

std::string ResponseGenerator::resolve_full_path(const Http::Request* request, const ServerConfig *config)
{
	const RouteRule *rule = config->findRoute(request->method(), request->path());

	std::string root = get_pwd() + rule->root.toString();
	size_t npos = 0;
	if (root.find('*', npos))
		root.erase(root.length() - 2, 2);
	if (request->path() == "/")
		return root;
	return root + request->path();
}

HttpResponse ResponseGenerator::make_error_response(int error_code, const ServerConfig *config)
{
	(void)config;

	HttpResponse res;
	if (error_code == 404)
		res.status_code = "404 Not Found";
	else if (error_code == 403)
		res.status_code = "403 Forbidden";
	else
		res.status_code = "500 Internal Server Error";

	res.body = "<html><body><h1>" + res.status_code + "</h1></body></html>";
	return res;
}
