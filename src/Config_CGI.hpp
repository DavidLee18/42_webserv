#ifndef CONFIG_CGI_HPP
#define CONFIG_CGI_HPP

#include "ParsingUtils.hpp"
#include "file_descriptor.h"
#include "http_1_1.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

class Config_CGI
{
	private:
		std::string executable;
		std::map<std::string, std::string> env;
		double timeout;
		std::string err;

		std::string parse_env(const std::string&);
		std::string parse_CGI(FileDescriptor &fd, std::string line);
	public:
		Config_CGI() : executable(""), timeout(-1), err("No parse") {};
		Config_CGI(FileDescriptor &fd, std::string);
};

# endif