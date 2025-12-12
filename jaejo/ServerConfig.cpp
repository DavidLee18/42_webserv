#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : is_success(false) {}


ServerConfig::ServerConfig(std::ifstream& file) : serverResponseTime(3)
{
    is_success = set_ServerConfig(file);
    (void)serverResponseTime;
    (void)routes;
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::set_ServerConfig(std::ifstream& file)
{
    std::string line;

    while (std::getline(file, line))
    {
        if (is_tab_or_space(line, 0))
            continue;
        else if (is_tab_or_space(line, 1))
        {
            if (is_header(line))
                parse_header_line(file, line);
        }
        else
            break;
    }
    return (true);
}

bool ServerConfig::getis_succes(void) { return(is_success); }

//header method
bool ServerConfig::is_header(const std::string& line)
{
    std::string temp = trim_space(line);
    if (line.empty())
        return (false);
    if (temp[0] == '[' && temp[1] == ']')
        return (true);
    return (false);
}


bool ServerConfig::is_header_line(std::ifstream& file, std::string line)
{
    std::string temp(line);
    std::vector<std::string> key_value = string_split(temp, ":");

    if (key_value.size() != 2)
        return (false);
    
    std::string key = trim_space(key_value[0]);
    temp = key_value[1];
    if (!is_header_key(key) || !parse_header_value(temp, key))
        return (false);
    while(!temp.empty() && temp[temp.length() - 1] == ';' && getline(file, temp))
    {
        if (!is_tab_or_space(temp, 2))
            return (false);
        if (!parse_header_value(temp, key))
            return (false);
    }
    return (true);
}

bool ServerConfig::is_header_key(std::string& key)
{
    std::vector<std::string> temp;

    if (key.empty())
        return (false);
    temp = string_split(key, " ");
    if (temp.size() == 3)
        key = temp[2];
    return (true);
}

bool ServerConfig::parse_header_value(std::string value, const std::string key)
{
    if (value.empty())
        return (false);
    if (value[value.length() - 1] == ' ' || value[value.length() - 1] == '\t')
        return (false);
    std::vector<std::string> values = string_split(trim_space(value), ";");
    std::vector<std::string> temp;
    for (size_t i = 0; i < values.size(); i++)
    {
        values[i] = trim_space(values[i]);
        temp = string_split(values[i], " ");
        if (temp.size() == 1 && temp[0] == "\'nosniff\'")
            header[key];
        else if (temp.size() == 2)
        {
            if (temp[1].size() < 2) return false;
            if (temp[1][0] != '\'' || temp[1][temp[1].size() - 1] != '\'') return false;
            for (size_t j = 1; j + 1 < temp[1].size(); ++j)
                if (temp[1][j] == '\'') return false; 
            header[key][temp[0]] = temp[1];
            std::cout << "key: [" << key << "] value_key: [" << temp[0] << "] value: [" << header[key][temp[0]] << "]" << std::endl;
        }
        else
        {
            // std::cout << trim_space(value) << std::endl;
            return (false);
        }
    }
    return (true);
}

void ServerConfig::parse_header_line(std::ifstream& file, std::string line)
{
    std::string temp(line);

    if (is_header_line(file, line))
    {
        ;
    }
}
