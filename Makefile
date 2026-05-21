CXX				:= c++
CXXFLAGS_COMMON	:= -Wall -Werror -Wextra -Wconversion -std=c++98 -DCONFIG_DEBUG
CXXFLAGS		:= -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS	:= -fsanitize=address -g3 -O0 -fno-omit-frame-pointer -fno-inline
NAME			:= webserv

BUILD_DIR := build
SRC_DIR := src

MAIN		:= main.cpp
SERVER		:= Server.cpp Server_connections.cpp Server_io.cpp Server_dispatch.cpp \
				Client.cpp Response.cpp ResponseUtils.cpp ResponseErrors.cpp	\
				MultipartParser.cpp ResponseHandlers.cpp DefaultError.cpp Session.cpp	\
				AutoindexResponse.cpp
CONFIG		:= WebserverConfig.cpp ServerConfig.cpp RouteRule_CGI.cpp PathPattern.cpp ConfigError.cpp
CGI         := CgiAuthType.cpp CgiMetaVar.cpp EtcMetaVar.cpp ServerName.cpp ContentType.cpp \
				CgiDelegate.cpp CgiInput.cpp
CORE		:= Errors.cpp EPoll.cpp FileDescriptor.cpp utils.cpp 

SRC_DIRS	:= server config cgi_1_1 core
SRCS		:= $(MAIN) $(CONFIG) $(SERVER) $(CGI) $(CORE)

OBJS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.d))

vpath %.cpp $(addprefix $(SRC_DIR)/,$(SRC_DIRS)) $(SRC_DIR)

all: $(NAME) cgi

cgi:
	$(TESTS_DIR)/cgi_setup.zsh
	$(TESTS_DIR)/multi_cgi_setup.zsh

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(NAME)

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean cgiclean
	rm -f $(NAME)

re:	fclean all

compile-commands:
	bear -- make re

cgiclean:
	rm -rf www-files/cgi-bin/

-include $(DEPS)

.PHONY: all clean fclean re bonus rebo cgi compile-commands

# -----------------------------------------------------------------------------
# Test suites
# -----------------------------------------------------------------------------
TESTS_DIR    := tests
TEST_SUITES  := parsing headers cgi_framing cgi chunked disconnect \
                conditional slowloris exhaustion eval_smoke multi_cgi

# Run every suite in sequence. Assumes webserv is already running on
# HOST:PORT (defaults 127.0.0.1:8080). Override via:
#   make test HOST=... PORT=... PYTHON=...
test: $(NAME)
	@$(TESTS_DIR)/run_all.zsh

# Run a single suite, e.g. `make test-chunked` or `make test-cgi_framing`.
test-%: $(NAME)
	@$(TESTS_DIR)/webserv_$*_tests.zsh

.PHONY: test $(addprefix test-,$(TEST_SUITES))
