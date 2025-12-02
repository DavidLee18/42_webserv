#ifndef WEBSERVERCONFIG_HPP
#define WEBSERVERCONFIG_HPP

#include "ParsingUtils.hpp"

// class ServerConfig
// {
//     private:

//     public:
//         ServerConfig(std::ifstream& file);
//         ~ServerConfig();
//         const ServerConfig& getServerConfig();
// };

class WebserverConfig
{
    private:
        std::string default_mime;
        std::map<std::string, std::string> type_map;
        // std::map<std::string, ServerConfig> server_map;

        bool file_parsing(std::ifstream& file);
        //type method
        bool set_type_map(std::ifstream& file);
        bool parse_type_line(const std::string& line,
                            std::vector<std::string>& keys_out,
                            std::string& value_out);
        std::vector<std::string> is_typeKey(const std::string& key);
        bool is_typeValue(const std::string& value);

    public:
        WebserverConfig(std::string config_file_name);
        ~WebserverConfig();
};

#endif