#include <iostream>
#include <atomic>
#include <thread>
#include <string>

#include <unistd.h>
#include <signal.h>

#include "server.h"


class Error
{
    std::string m_message;
public:
    Error() : m_message("") {}
    Error(const char* message) : m_message(message) {}

    operator bool() const noexcept {
        return m_message != "";
    }

    std::string string() const noexcept {
        return m_message;
    }
};


std::tuple<int, Error>
parsePort(std::string s)
{
    try {
        int port = std::stoi(s);
        if (port < 1 || port > 0xffff) {
            return std::make_tuple(0, "invalid port");
        }
        return std::make_tuple(port, Error());
    } 
    catch (std::exception& e) {
        return std::make_tuple(0, "invalid port");
    }
}


std::tuple<std::string, int, Error> 
parseAddr(std::string s)
{
    std::size_t colonPos = s.find(':');
    if (colonPos == std::string::npos) {
        return std::make_tuple("", 0, "invalid address");
    }
    std::string host = s.substr(0, colonPos);
    auto [port, err] = parsePort(s.substr(colonPos + 1));
    return std::make_tuple(host, port, err);
}


sigset_t configureExitSignals()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    struct sigaction act;
    act.sa_handler = [](int) {};
    act.sa_mask = set;
    act.sa_flags = 0;

    sigaction(SIGINT, &act, nullptr);
    sigaction(SIGTERM, &act, nullptr);

    return set;
}


void printUsage() {
    std::cerr << "Usage: <-b port> <-i addr> [-s]" << std::endl;
}


int main(int argc, char* argv[])
{

    int portToListen = 0;
    std::string host = "";
    int port         = 0;
    bool enableSSL   = false;

    int opt;
    Error err;
    while((opt = getopt(argc, argv, "b:i:sh")) != -1) 
    {
        switch (opt) {
        case 'h':
            printUsage();
            exit(EXIT_SUCCESS);
        case 'b':
            std::tie(portToListen, err) = parsePort(optarg);
            if (err) {
                std::cout << argv[0] << ": " << err.string() << std::endl;
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            std::tie(host, port, err) = parseAddr(optarg);
            if (err) {
                std::cout << argv[0] << ": " << err.string() << std::endl;
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            enableSSL = true;
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }

    if (portToListen == -1 || host == "") {
        printUsage();
        exit(EXIT_FAILURE);
    }

    Server server(portToListen, host, port);

    sigset_t set = configureExitSignals();

    std::thread th([&server, enableSSL] {
        try {
            if (enableSSL) {
                server.listenAndServeTLS("certs/rootCA.crt");
            } else {
                server.listenAndServe();
            }
        }
        catch (ServerException& e) {
            std::cerr << e.what() << std::endl;
        }
        kill(getpid(), SIGTERM);
    });

    int sig;
    sigwait(&set, &sig);

    server.shutdown();
    th.join();

    return 0;
}