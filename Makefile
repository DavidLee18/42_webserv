CXX				:= c++
CXXFLAGS_COMMON	:= -Wall -Werror -Wextra -Wconversion -std=c++98
CXXFLAGS		:= -O2 -foptimize-sibling-calls
DEBUG_CXXFLAGS	:= -fsanitize=address -g3 -O0 -fno-omit-frame-pointer -fno-inline
NAME			:= webserv

BUILD_DIR := build
SRC_DIR := src


SRC_FILES	:= errors.cpp epoll_kqueue.cpp file_descriptor.cpp \
	utils.cpp json.cpp cgi_1_1.cpp main.cpp
SERVER		:= Server.cpp Client.cpp Response.cpp DefaultError.cpp Session.cpp
CONFIG		:= WebserverConfig.cpp ServerConfig.cpp RouteRule_CGI.cpp PathPattern.cpp

SRC_DIRS	:= server config
SRCS		:= $(SRC_FILES) $(CONFIG) $(SERVER)

OBJS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
DEPS		:= $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.d))

vpath %.cpp $(addprefix $(SRC_DIR)/,$(SRC_DIRS)) $(SRC_DIR)

CGI_NAME      := www-files/cgi-bin/gen_html.cgi
CGI_SRC       := src/cgi/cgi_html_gen.cpp

all: $(NAME) cgi

test-cgi: $(NAME) cgi
	@echo "── CGI framing parser unit tests ──────────────────────────────"
	@cd tests && PROJECT_ROOT=$(CURDIR) zsh ./webserv_cgi_framing_tests.zsh
	@echo ""
	@echo "── CGI sandboxing integration tests ───────────────────────────"
	@echo "NOTE: start ./$(NAME) <config> in another terminal first."
	@cd tests && zsh ./webserv_cgi_tests.zsh

cgi: $(CGI_NAME)
	$(TESTS_DIR)/cgi_setup.zsh

$(CGI_NAME): $(CGI_SRC)
	mkdir -p www-files/cgi-bin/
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

re:	fclean all

compile-commands:
	bear -- make re

cgiclean:
	rm -rf www-files/cgi-bin/

-include $(DEPS)

.PHONY: all clean fclean re bonus rebo cgi test-cgi compile-commands

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
