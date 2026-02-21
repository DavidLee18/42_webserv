CXX := c++
CXXFLAGS_COMMON := -Wall -Werror -Wextra -Wconversion -std=c++98
CXXFLAGS := -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS := -g3 -O0 #-fsanitize=address -fno-omit-frame-pointer
NAME := webserv

BUILD_DIR := build
SRC_DIR := src

SRC_FILES := errors.cpp epoll_kqueue.cpp file_descriptor.cpp	\
	ParsingUtils.cpp ServerConfig.cpp WebserverConfig.cpp		\
	json.cpp cgi_1_1.cpp wsgi.cpp http_1_1.cpp					\
	ServerSocket.cpp 											\
	main.cpp 
SRCS := $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS := $(addprefix $(BUILD_DIR)/, $(SRC_FILES:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(NAME)

build/%.o: src/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -c $< -o $@

$(OBJS): $(SRC_DIR)/webserv.h

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean
	rm -f $(NAME)

re:	fclean all

.PNONY: all clean fclean re bonus rebo
