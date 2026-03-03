#ifndef SESSION_HPP
#define SESSION_HPP

#include <string>

struct ClientSession
{
	std::string in_buff;
	std::string out_buff;
	// todo: RequestState
};

#endif