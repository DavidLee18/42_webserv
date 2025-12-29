CXX := c++
CXXFLAGS := -Wall -Werror -Wextra -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS := -Wall -Werror -Wextra -g #-fsanitize=address -O1 -fno-omit-frame-pointer
NAME := webserv

BUILD_DIR := build
SRC_DIR := src

SRCS := src/errors.cpp src/epoll_kqueue.cpp \
	src/file_descriptor.cpp src/json.cpp src/main.cpp
OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(DEBUG_CXXFLAGS) -o $(NAME)

build/%.o: src/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(DEBUG_CXXFLAGS) -c $< -o $@

$(SRCS): $(SRC_DIR)/webserv.h

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean
	rm -f $(NAME)

re:	fclean all

.PNONY: all clean fclean re bonus rebo
