CXX				:= c++
CXXFLAGS_COMMON	:= -Wall -Werror -Wextra -Wconversion -std=c++98
CXXFLAGS		:= -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS	:= -g3 -O0 #-fsanitize=address -fno-omit-frame-pointer
NAME			:= webserv

BUILD_DIR := build
SRC_DIR := src

UWSGI_NAME     := uwsgi_server
UWSGI_SRC_DIR  := src/uwsgi_server
UWSGI_BUILD_DIR := build/uwsgi_server
UWSGI_SRC_FILES := uwsgi_server.cpp main.cpp
UWSGI_SRCS     := $(addprefix $(UWSGI_SRC_DIR)/, $(UWSGI_SRC_FILES))
UWSGI_OBJS     := $(addprefix $(UWSGI_BUILD_DIR)/, $(UWSGI_SRC_FILES:.cpp=.o))
UWSGI_DEPS     := $(addprefix $(UWSGI_BUILD_DIR)/, $(UWSGI_SRC_FILES:.cpp=.d))

SRC_FILES	:= errors.cpp epoll_kqueue.cpp file_descriptor.cpp	\
	ParsingUtils.cpp ServerConfig.cpp WebserverConfig.cpp		\
	json.cpp cgi_1_1.cpp wsgi.cpp http_1_1.cpp					\
	main.cpp 
SERVER		:=	Server.cpp	Session.cpp	Response.cpp

SRC_DIRS	:= server
SRCS		:= $(SRC_FILES) $(SERVER)

OBJS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.d))

vpath %.cpp $(addprefix $(SRC_DIR)/,$(SRC_DIRS)) $(SRC_DIR)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(NAME)

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean
	rm -f $(NAME)
	rm -f $(UWSGI_NAME)

re:	fclean all

uwsgi: $(UWSGI_NAME)

$(UWSGI_NAME): $(UWSGI_OBJS)
	$(CXX) $(UWSGI_OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(UWSGI_NAME)

$(UWSGI_BUILD_DIR)/%.o: $(UWSGI_SRC_DIR)/%.cpp
	mkdir -p $(UWSGI_BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)
-include $(UWSGI_DEPS)

.PHONY: all clean fclean re bonus rebo uwsgi
