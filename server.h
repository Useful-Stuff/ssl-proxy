#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <set>

#include "selector.h"

class IConnection
{
public:
    virtual ~IConnection() {}
    virtual void close() = 0;
};

class Server
{
    static const int BACKLOG = 16;

    int         m_portToListen;
    std::string m_host;
    int         m_port;
    bool        m_enableSSL;
    
    Selector m_selector;
    int      m_listener;

    std::set<IConnection*> m_connections;

public:
    Server(int portToListen, const std::string& host, int port);
    ~Server() = default;

    void listenAndServe();
    void listenAndServeTLS(const char* CAfile);
    void shutdown();

    void removeConnection(IConnection* conn);
private:
    void do_accept();
    void do_connect(int client);

    IConnection* createConnection(int client, int server);
    void closeConnections();
};


class ServerException : public std::exception
{
    std::string m_message;
public:
    ServerException(const char* message) : m_message(message) {}

    virtual const char* what() const noexcept override {
        return m_message.c_str();
    }
};


#endif // SERVER_H