#include "core/FileDescriptor.hpp"
#include "config/WebserverConfig.hpp"
#include "server/Server.hpp"
#include <csignal>
#include <iostream>

volatile sig_atomic_t g_receivedSignal = 0;

void wrap_up(const int signum) throw() { g_receivedSignal = signum; }

int  main(const int argc, char* argv[], char** envp) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, wrap_up);
    if (argc != 2) {
        std::cerr << "Usage: webserv <config_file>" << std::endl;
        return 1;
    }
    Result<FileDescriptor> fd = FileDescriptor::open_file(argv[1], envp);
    PANIC(fd)
    const Result<WebserverConfig> result_config =
        WebserverConfig::parse(fd.value_mut(), envp);
    PANIC(result_config)

#ifdef CONFIG_DEBUG
    std::cout << result_config.value() << std::endl;
#else
    const WebserverConfig& config = result_config.value();
    Server                 server(config);
    const Result<Void>     init_result = server.init();
    PANIC(init_result)
    const Result<Void> server_result = server.start(envp);
    PANIC(server_result)
    std::cout << std::endl << utils::info << "Wrapping up..." << std::endl;
#endif
    return 0;
}
