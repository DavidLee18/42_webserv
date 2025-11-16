#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

class Webserver
{
    private:
        std::string default_mime;
        std::map<std::string, std::string> type_map;

        bool set_type_map(std::ifstream& file);
        bool file_parsing(std::ifstream& file);
    public:
        Webserver(std::string config_file_name);
        ~Webserver();
};

#endif