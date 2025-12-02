#ifndef PARSINGUTILS_HPP
#define PARSINGUTILS_HPP

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

bool is_have_space(const std::string& line);
std::string trim_space(const std::string &s);
bool is_tab_or_space(std::string line, int num);
std::vector<std::string> string_split(const std::string &line, const std::string &delim);
bool is_have_special(const std::string &line, const std::string &allowed);
int number_of_delim(const std::string& line, const std::string& delim);


#endif