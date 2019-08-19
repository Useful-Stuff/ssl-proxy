#include <sys/types.h>
#include <sys/socket.h>

#include "server.h"
#include "selector.h"


class Connection : public IConnection
{
    static const int BUFSIZE = 1024;

    Server   *m_server;
    Selector *m_selector;

    int  m_clientSocket;
    int  m_serverSocket;
    char m_buf[BUFSIZE];
    int  m_len;

public:
    Connection(Server* serv, Selector* sel, int clientSock, int serverSock)
        : m_server(serv), m_selector(sel), 
          m_clientSocket(clientSock), m_serverSocket(serverSock) 
    {
        do_read(m_clientSocket);
    }

    ~Connection() = default;

    virtual void close() override;

private:
    void do_read(int sock);
    void do_write(int sock);

    int getPeer(int sock) const noexcept {
        return sock == m_clientSocket ? m_serverSocket : m_clientSocket;
    }
};


void Connection::close()
{
    shutdown(m_clientSocket, SHUT_RDWR);
    shutdown(m_serverSocket, SHUT_RDWR);
    ::close(m_clientSocket);
    ::close(m_serverSocket);
    m_server->removeConnection(this);
}


void Connection::do_read(int sock)
{
    m_selector->addReadEvent(sock, 
    [this](int sock) 
    {
        m_len = recv(sock, m_buf, BUFSIZE, 0);
        if (m_len > 0) {
            return do_write(getPeer(sock));
        }
        this->close();
    });
}


void Connection::do_write(int sock)
{
    m_selector->addWriteEvent(sock,
    [this](int sock)
    {
        m_len = send(sock, m_buf, m_len, 0);
        if (m_len > 0) {
            return do_read(sock);
        }
        this->close();
    });
}