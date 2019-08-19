#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "server.h"
#include "connection.h"
#include "ssl_connection.h"


int createNonblockingSocket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl");
        close(sock);
        return -1;
    }

    return sock;
}


int createServerSocket(const char* host, int port, int backlog)
{
    int sock = createNonblockingSocket();
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!inet_aton(host, &addr.sin_addr)) {
        perror("inet_aton");
        return -1;
    }

    if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    if (listen(sock, backlog) < 0) {
        perror("listen");
        close(sock);
        return -1;
    } 

    return sock;
}


int connect(int sock, const char* host, int port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!inet_aton(host, &addr.sin_addr)) {
        perror("inet_aton");
        return -1;
    }

    int err = connect(sock, (struct sockaddr*) &addr, sizeof(addr));
    if (err == -1) {
        return errno;
    }
    return 0;
}


int getConnectResult(int sock)
{
    int err;
    socklen_t err_len;
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR,  &err, &err_len) != 0) {
        perror("getsockopt");
        throw 28;
    }
    return err;
}

Server::Server(int portToListen, const std::string& host, int port)
    : m_portToListen(portToListen), 
      m_host(host), m_port(port), m_enableSSL(false) {}


void Server::listenAndServe()
{
    if (m_host == "localhost") {
        m_host = "127.0.0.1";
    }
    in_addr addr;
    if (!inet_aton(m_host.c_str(), &addr)) {
        throw ServerException("invalid address");
    }

    m_listener = createServerSocket("127.0.0.1", m_portToListen, BACKLOG);
    if (m_listener < 0) {
        return;
    }
    do_accept();
    m_selector.run();
    closeConnections();
}


void Server::shutdown() {
    m_selector.stop();
}


void Server::do_accept()
{
    m_selector.addReadEvent(m_listener, 
        [this](int) 
        {
            int client = accept4(m_listener, nullptr, nullptr, SOCK_NONBLOCK);
            if (client < 0) {
                perror("accept");
                return;
            }     
            do_connect(client);
            this->do_accept();
        });
}


void Server::do_connect(int client)
{

    int server = createNonblockingSocket();
    if (server < 0) {
        ::shutdown(client, SHUT_RDWR);
        close(client);
        return;
    }

    int err = connect(server, m_host.c_str(), m_port);

    if (err < 0 && err != EINPROGRESS) {
        ::shutdown(client, SHUT_RDWR);
        close(client);
        close(server);
    }

    m_selector.addWriteEvent(server,
        [this, client](int server) 
        {
            if (getConnectResult(server) != 0) 
            {
                ::shutdown(client, SHUT_RDWR);
                ::shutdown(server, SHUT_RDWR);
                close(client);
                close(server);
                return;
            }
            IConnection *conn = createConnection(client, server);
            m_connections.insert(conn);
        });
}


IConnection* Server::createConnection(int client, int server)
{
    if (m_enableSSL) {
        return new SSLConnection(this, &m_selector, client, server);
    }
    return new Connection(this, &m_selector, client, server);
}


void Server::listenAndServeTLS(const char* CAfile)
{
    try {
        SSLConnection::init(CAfile); 
        m_enableSSL = true;
        listenAndServe();
        SSLConnection::free();
    } 
    catch (SSLException& e) {
        throw ServerException(e.what());
    }
    closeConnections();
}


void Server::removeConnection(IConnection* conn) 
{
    m_connections.erase(conn);
    delete conn;
}

void Server::closeConnections() 
{
    for (IConnection* conn : m_connections) {
        conn->close();
    }
}