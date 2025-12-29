cmake_minimum_required(VERSION 3.13)
project(42_webserv)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -g")
set(SOURCE_FILES
        src/errors.h
        src/result.h
        src/map_record.h
        src/http_1_1.h
        src/vec.h
        src/json.h
        src/file_descriptor.h
        src/epoll_kqueue.h
        src/webserv.h
        src/epoll_kqueue.cpp
        src/file_descriptor.cpp
        src/json.cpp
        src/main.cpp
)
add_executable(42_webserv ${SOURCE_FILES})
