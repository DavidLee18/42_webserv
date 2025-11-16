#include "Webserver.hpp"

static std::string trim_space(const std::string &s)
{
    std::size_t start = s.find_first_not_of(" \t");
    std::size_t end   = s.find_last_not_of(" \t");

    if (start == std::string::npos)
        return "";

    return s.substr(start, end - start + 1);
}

static bool is_tab_or_space(std::string line, int num)
{
    int len = 0;
    int i = 0;

    while (line[i] == ' ' || line[i] == '\t')
    {
        if (line[i] == ' ')
            len += 1;
        else
            len += 4;
        i++;
    }
    if (len == num * 4)
        return (true);
    return (false);
}

static std::vector<std::string> string_split(const std::string &line, const std::string &delim)
{
    std::vector<std::string> tokens;
    std::size_t start = 0;
    std::size_t end;

    while ((end = line.find(delim, start)) != std::string::npos && delim.length() != 0)
    {
        tokens.push_back(line.substr(start, end - start));
        start = end + delim.size();
    }
    tokens.push_back(line.substr(start));

    return tokens;
}

bool Webserver::set_type_map(std::ifstream& file)
{
    std::string key;
    std::string line;
    std::string value;
    const std::string delim = "->";
    std::vector<std::string> map_data;
    std::vector<std::string> temp;

    std::getline(file, line);
    while (is_tab_or_space(line, 1))
    {
        std::size_t pos = line.find(delim);
        if (pos == std::string::npos)
            return (false);
        map_data = string_split(line, delim);
        if (map_data.size() != 2)
            return (false);
        value = map_data[1];
        temp = string_split(trim_space(map_data[1]), "/");
        if (temp.size() != 2)
            return (false);
        temp = string_split(map_data[0], "|");
        for (size_t i = 0; i < temp.size(); i++)
        {
            key = trim_space(temp[i]);
            if (key != "_" && type_map.find(key) == type_map.end())
                type_map[key] = trim_space(value);
            else if (type_map.find(key) != type_map.end())
                return (false);
            if (key == "_")
                default_mime = trim_space(value);
        }
        std::getline(file, line);
    }
    return (true);
}

Webserver::Webserver(std::string config_file_name)
{
    if (config_file_name == "../default.wbsrv")
    {
        std::ifstream file(config_file_name.c_str());
        if (!file.is_open())
        {
            std::cerr   << "Error: Config file open err" << std::endl;
            return;
        }
        if (!this->file_parsing(file))
        {
            std::cerr   << "Error: Error while parsing the file." << std::endl;
            return;
        }
        file.close();
        std::cout   << "Config file parsing succeeded." << std::endl;
        return;
    }
    std::cerr   << "Error: This is not a config file." << std::endl;
}

Webserver::~Webserver() {};

bool Webserver::file_parsing(std::ifstream& file)
{
    std::string line;

    while (std::getline(file, line) && is_tab_or_space(line, 0))
    {
        if (line == "types =" || line == "types=")
            if (!set_type_map(file))
                return (false);
    }
    if (this->type_map.empty() || this->default_mime.length() == 0)
        return (false);
    for (std::map<std::string, std::string>::iterator it = this->type_map.begin();
         it != this->type_map.end();
         ++it)
    {
        std::cout << it->first << " = " << it->second << std::endl;
    }
    std::cout << default_mime << std::endl;

    return (true);
}