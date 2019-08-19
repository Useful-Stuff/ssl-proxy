#include <sys/types.h>
#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "server.h"
#include "selector.h"


class SSLException : public std::exception
{
    std::string m_message;
public:
    SSLException(const char* msg) : m_message(msg) 
    {
        char errstr[256];
        ERR_error_string(ERR_get_error(), errstr);
        m_message += ": " + std::string(errstr);
    }

    SSLException(const char* msg, SSL* ssl, int errcode) : m_message(msg) 
    {
        char errstr[256];
        errcode = SSL_get_error(ssl, errcode);
        ERR_error_string(errcode, errstr);
        m_message += ": " + std::string(errstr);
    }

    virtual const char* what() const noexcept override {
        return m_message.c_str();
    }
};


class SSLConnection : public IConnection
{
private:
    static SSL_CTX *ctx;
public:
    static void init(const char* CAfile);
    static void free();

private:
    static const int BUFSIZE = 1024;

    Server   *m_server;
    Selector *m_selector;
    
    SSL *m_ssl;

    int  m_clientSocket;
    int  m_serverSocket;
    char m_buf[BUFSIZE];
    int  m_len;

public:
    SSLConnection(Server* serv, Selector* sel, int clientSock, int serverSock);
    ~SSLConnection() = default;

    virtual void close() override;

private:
    void do_connect_ssl();
    void do_read();
    void do_write();
    void do_read_ssl();
    void do_write_ssl();
};


SSL_CTX *SSLConnection::ctx = nullptr;


void SSLConnection::init(const char* CAfile)
{
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw SSLException("SSL_CTX_new");
    }

    if (!SSL_CTX_load_verify_locations(ctx, CAfile, nullptr)) {
        throw SSLException("Cannot load certificates");
    }
}


void SSLConnection::free() {
    SSL_CTX_free(ctx);
}


SSLConnection::SSLConnection(
    Server* serv, Selector* sel, int clientSock, int serverSock)
    : m_server(serv), m_selector(sel), 
      m_clientSocket(clientSock), m_serverSocket(serverSock)
{
    m_ssl = SSL_new(ctx);
    if (!m_ssl) {
        throw SSLException("SSL_new");
    }

    if (!SSL_set_fd(m_ssl, serverSock)) {
        throw SSLException("SSL_set_fd");
    }

    do_connect_ssl();
}


void SSLConnection::close()
{
    shutdown(m_clientSocket, SHUT_RDWR);
    shutdown(m_serverSocket, SHUT_RDWR);
    ::close(m_clientSocket);
    ::close(m_serverSocket);
    m_server->removeConnection(this);
}


void SSLConnection::do_connect_ssl()
{
    int ret = SSL_connect(m_ssl);
    if (ret == 1) {
        return do_read();
    }

    int err = SSL_get_error(m_ssl, ret);

    if (err == SSL_ERROR_WANT_READ) {
        m_selector->addReadEvent(m_serverSocket, [this](int) { 
            do_connect_ssl(); 
        });
    }
    else if (err == SSL_ERROR_WANT_WRITE) {
        m_selector->addWriteEvent(m_serverSocket, [this](int) {
            do_connect_ssl();
        });
    }
    else {
        throw SSLException("SSL_connect", m_ssl, ret);
    }
}


void SSLConnection::do_read()
{
    m_selector->addReadEvent(m_clientSocket, 
    [this](int sock) 
    {
        m_len = recv(sock, m_buf, BUFSIZE, 0);

        if (m_len > 0) {
            return do_write_ssl();
        }

        if (m_len == 0) {
            // SSL shutdown
            this->close();
        }

        if (m_len < 0) {
            this->close();
        } 
    });
}


void SSLConnection::do_write()
{
    m_selector->addWriteEvent(m_clientSocket,
    [this](int sock)
    {
        m_len = send(sock, m_buf, m_len, 0);
        if (m_len > 0) {
            return do_read();
        }

        if (m_len == 0) {
            // SSL shutdown
            this->close();
        }    

        if (m_len < 0) {
            this->close();
        }
    });
}


void SSLConnection::do_read_ssl()
{
    int n = SSL_read(m_ssl, m_buf, BUFSIZE);
    if (n > 0) {
        m_len = n;
        return do_write();
    }
    
    int err = SSL_get_error(m_ssl, n);

    if (err == SSL_ERROR_WANT_READ) {
        m_selector->addReadEvent(m_serverSocket, [this](int) {
            do_read_ssl();
        });
    }
    else if (err == SSL_ERROR_WANT_WRITE) {
        m_selector->addWriteEvent(m_serverSocket, [this](int) {
            do_read_ssl();
        });
    }
    else {
        throw SSLException("SSL_read", m_ssl, n);
    }
}


void SSLConnection::do_write_ssl()
{
    int n = SSL_write(m_ssl, m_buf, m_len);
    if (n > 0) {
        return do_read_ssl();
    }
    
    int err = SSL_get_error(m_ssl, n);

    if (err == SSL_ERROR_WANT_READ) {
        m_selector->addReadEvent(m_serverSocket, [this](int) {
            do_write_ssl();
        });
    }
    else if (err == SSL_ERROR_WANT_WRITE) {
        m_selector->addWriteEvent(m_serverSocket, [this](int) {
            do_write_ssl();
        });
    }
    else {
        throw SSLException("SSL_write", m_ssl, n);
    }
}