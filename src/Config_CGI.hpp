#ifndef CONFIG_CGI_HPP
#define CONFIG_CGI_HPP

#include "ParsingUtils.hpp"
#include "file_descriptor.h"
#include "http_1_1.h"

class Config_CGI
{
	private:
		std::string executable;
		std::map<std::string, std::string> env;
		int timeout;

	public:
		Config_CGI() : timeout(-1);
		Config_CGI(FileDescriptor &fd, std::string);
};

#endif