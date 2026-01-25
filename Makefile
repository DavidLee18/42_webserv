CXX := c++
CXXFLAGS_COMMON := -Wall -Werror -Wextra -Wconversion -std=c++98
CXXFLAGS := -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS := -g3 -O0 #-fsanitize=address -fno-omit-frame-pointer
NAME := webserv

BUILD_DIR := build
SRC_DIR := src

SRCS := src/errors.cpp src/epoll_kqueue.cpp \
	src/ParsingUtils.cpp src/ServerConfig.cpp src/WebserverConfig.cpp \
	src/file_descriptor.cpp src/json.cpp src/cgi_1_1.cpp src/http_1_1.cpp src/main.cpp
OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(NAME)

build/%.o: src/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -c $< -o $@

$(SRCS): $(SRC_DIR)/webserv.h

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean
	rm -f $(NAME)

re:	fclean all

.PNONY: all clean fclean re bonus rebo
