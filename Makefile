CXX				:= c++
CXXFLAGS_COMMON	:= -Wall -Werror -Wextra -Wconversion -std=c++98
CXXFLAGS		:= -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS	:= -fsanitize=address -g3 -O0 -fno-omit-frame-pointer -fno-inline
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


SRC_FILES	:= errors.cpp epoll_kqueue.cpp file_descriptor.cpp \
	utils.cpp json.cpp cgi_1_1.cpp uwsgi.cpp uwsgi_client.cpp \
	main.cpp
SERVER		:= Server.cpp Client.cpp Response.cpp DefaultError.cpp Session.cpp
CONFIG		:= WebserverConfig.cpp ServerConfig.cpp RouteRule_CGI.cpp PathPattern.cpp

SRC_DIRS	:= server config
SRCS		:= $(SRC_FILES) $(CONFIG) $(SERVER)

OBJS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.d))

vpath %.cpp $(addprefix $(SRC_DIR)/,$(SRC_DIRS)) $(SRC_DIR)

CGI_NAME      := spool/www/cgi-bin/gen_html.cgi
CGI_SRC       := src/cgi/cgi_html_gen.cpp

all: $(NAME) uwsgi cgi

integration-test: all
	bash tests/integration/cgi_uwsgi_full_suite.sh

test-cgi: $(NAME) cgi
	@echo "── CGI framing parser unit tests ──────────────────────────────"
	@cd tests && PROJECT_ROOT=$(CURDIR) zsh ./webserv_cgi_framing_tests.zsh
	@echo ""
	@echo "── CGI sandboxing integration tests ───────────────────────────"
	@echo "NOTE: start ./$(NAME) <config> in another terminal first."
	@cd tests && zsh ./webserv_cgi_tests.zsh

cgi: $(CGI_NAME)

$(CGI_NAME): $(CGI_SRC)
	mkdir -p spool/www/cgi-bin/
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(CGI_NAME) $(CGI_SRC)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(NAME)

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -I$(SRC_DIR) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

fclean:	clean cgiclean
	rm -f $(NAME)
	rm -f $(UWSGI_NAME)

re:	fclean all

uwsgi: $(UWSGI_NAME)

$(UWSGI_NAME): $(UWSGI_OBJS)
	$(CXX) $(UWSGI_OBJS) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -o $(UWSGI_NAME)

$(UWSGI_BUILD_DIR)/%.o: $(UWSGI_SRC_DIR)/%.cpp
	mkdir -p $(UWSGI_BUILD_DIR)
	$(CXX) $(CXXFLAGS_COMMON) $(DEBUG_CXXFLAGS) -MMD -MP -c $< -o $@

compile-commands:
	bear -- make re

cgiclean:
	rm -rf spool/www/cgi-bin/

-include $(DEPS)
-include $(UWSGI_DEPS)

.PHONY: all clean fclean re bonus rebo uwsgi cgi integration-test test-cgi compile-commands

# -----------------------------------------------------------------------------
# Test suites
# -----------------------------------------------------------------------------
TESTS_DIR    := tests
TEST_SUITES  := parsing headers cgi_framing cgi chunked disconnect \
                conditional slowloris exhaustion

# Run every suite in sequence. Assumes webserv is already running on
# HOST:PORT (defaults 127.0.0.1:8080). Override via:
#   make test HOST=... PORT=... PYTHON=...
test: $(NAME)
	@$(TESTS_DIR)/run_all.zsh

# Run a single suite, e.g. `make test-chunked` or `make test-cgi_framing`.
test-%: $(NAME)
	@$(TESTS_DIR)/webserv_$*_tests.zsh

.PHONY: test $(addprefix test-,$(TEST_SUITES))
