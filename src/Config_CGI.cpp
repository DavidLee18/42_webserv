#include "Config_CGI.hpp"

Config_CGI::Config_CGI(FileDescriptor &fd, std::string line)
{
	std::string temp = trim_char(line, '$');
	std::size_t pos = temp.find('(');

	executable = temp.substr(0, pos);
}