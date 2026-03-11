#include "Config_CGI.hpp"

Config_CGI::Config_CGI(FileDescriptor &fd, std::string line)
{
	err = "";
	timeout = 3;
	err = parse_CGI(fd, line);
}


std::string Config_CGI::parse_CGI(FileDescriptor &fd, std::string line)
{
	std::string err_meg = "";
	
	if (!is_CGI(line))
		return "CGI Error: \"" + line + "\" CGI executable not found";
	std::string file_line = trim_char(line, '$');
	std::size_t start = file_line.find('(');
	if (std::string::npos != start)
	{
		std::size_t end = file_line.find(')');
		executable = file_line.substr(0, start);
		std::string env = file_line.substr(start + 1, end - start - 1);
		err_meg = parse_env(env);
		if (err_meg != "")
			return err_meg;	
	}
	else
		executable = file_line;
	while (true)
	{
		Result<std::string> temp = fd.read_file_line();
		if (temp.error() != "")
			return "FileDescriptor Error: " + temp.error();
		else if (temp.value() == "\n" || temp.value() == "")
			return "";
		file_line = trim_char(temp.value(), '\n');
		if (is_tab_or_space(file_line, 2) == false || (file_line.empty() || 
			file_line[file_line.length() - 1] == ' ' || file_line[file_line.length() - 1] == '\t' ))
      		return "Error: \"" + file_line + "\" Indentation or space error";
		file_line = trim_space(file_line);
		if (is_timeout(file_line))
			timeout = parse_timeout(file_line);
		else if (std::string::npos != file_line.find("="))
		{
			err_meg = parse_env(file_line);
			if (err_meg != "")
				return err_meg;	
		}
	}
}

static bool isExecutableFile(const std::string& path)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    if (!S_ISREG(st.st_mode))
        return false;

    return access(path.c_str(), X_OK) == 0;
}

static bool is_CGI(const std::string& line)
{
	std::size_t i = 1;

	if (line[0] != '$' || is_have_space(line))
		return false;
	
	std::size_t pos = line.find(".cgi");	
	if (std::string::npos != pos)
	{
		i = pos + 4;
		if (!isExecutableFile(line.substr(1, pos + 3)))
			return false;
	}
	else
	{
		while (i < line.length() && line[i] !='(')
		{
			if(std::isdigit(line[i]))
				i++;
			else
				return false;
		}
	}
	if (i < line.length() && line[i] == '(')
	{
		std::size_t end = line.find(')');
		if (std::string::npos == end)
			return false;
		if (end + 1 != line.length())
			return false;
	}
	return true;
}

static bool is_timeout(const std::string& line)
{
	if (line.length() < 4 || line[0] != '.' || line[1] != '.' || line[2] != '.')
		return false;
	double data = 0;
	bool dot = false;
	for (size_t i = 3; i < line.length(); ++i) {
		if (line[i] == '.') {
			if (dot)
				return false;
			dot = true;
		}
		else if (!std::isdigit(static_cast<unsigned char>(line[i])))
			return false;
	}
	std::stringstream ss(line.substr(3));
	if (!(ss >> data))
		return false;
	if (data > 15.0 || 0.05 >= data)
		return false;
	return true;
}

static double parse_timeout(std::string line)
{
	double time = 3;
	std::stringstream oss;
  	oss << line.erase(0, 3);
  	oss >> time;
	return time;
}

static bool is_key(const std::string& key)
{
	std::size_t i = 0;

	while (i < key.length())
	{
		if (std::isupper(key[i]) || key[i] == '_' ||
			(i != 0 && std::isdigit(static_cast<unsigned char>(key[i]))))
			i++;
		else
			return false;
	}
	return true;
}

std::string Config_CGI::parse_env(const std::string& env)
{
	std::vector<std::string> key_and_value = string_split(env, "=");
	if (key_and_value.size() != 2)
		return "Error: \"" + env + "\" Invalid environment variable syntax";
	if (!is_key(key_and_value[0]))
		return "Error: \"" + key_and_value[0] + "\" Invalid environment variable value";
	this->env[key_and_value[0]] = key_and_value[1];
	return "";
}