#include "Response.hpp"

static int check_path_type(const std::string &path)
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
	return 0;
}

HttpResponse PathRoute::get_file_content(const std::string path)
{
	HttpResponse response;

	std::string route = "./spool/www" + path;
	int path_type = check_path_type(route);

	if (path_type == IS_DIRECTORY)
		route += "/index.html";
	else if (path_type == NOT_FOUND)
	{
		response.status_code = "404 Not Found";
		response.body = "<html><body><h1>404 Error: File Not Found</h1></body></html>";
		return response;
	}
	else if (path_type == FORBIDDEN)
	{
		response.status_code = "403 Forbidden";
		response.body = "<html><body><h1>404 Error: Permission Denied</h1></body></html>";
		return response;
	}
	std::ifstream file(route.c_str());
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
