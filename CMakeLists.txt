cmake_minimum_required(VERSION 3.13)
project(42_webserv)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -g")
set(SOURCE_FILES
        src/main.cpp
        src/webserv.h
)
add_executable(42_webserv ${SOURCE_FILES})