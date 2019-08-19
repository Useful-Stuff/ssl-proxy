#include <iostream>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>


thread_local char errstr[256];

void print_ssl_error(const char* msg, SSL* ssl = nullptr, int errcode = 0)
{
    if (ssl)
        errcode = SSL_get_error(ssl, errcode);
    else
        errcode = ERR_get_error();
    ERR_error_string(errcode, errstr);
    std::cout << msg << ": " << errstr << std::endl;
}


int main()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8443);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_socket, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        return 2;
    }

    if (listen(server_socket, 1) < 0) {
        perror("listen");
        return 3;
    }

    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        print_ssl_error("cannot create ssl context");
        return 1;
    }

    if (!SSL_CTX_use_certificate_file(ctx, "certs/localhost.crt", SSL_FILETYPE_PEM)) {
        print_ssl_error("cannot load the certificate");
        return 2;
    }

    if (!SSL_CTX_use_PrivateKey_file(ctx, "certs/localhost.key", SSL_FILETYPE_PEM)) {
        print_ssl_error("cannot load the key");
        return 3;
    }

    while(true) 
    {
        int sock = accept(server_socket, nullptr, nullptr);
        if(sock < 0) {
            perror("accept");
            break;
        }

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            print_ssl_error("SSL_new");
            break;
        }

        if (!SSL_set_fd(ssl, sock)) {
            print_ssl_error("SSL_set_fd");
            break;
        }

        int err = SSL_accept(ssl);
        if (err <= 0) {
            print_ssl_error("ssl accept", ssl, err);
            continue;
        }
        
        std::thread th([ssl, sock]() 
        {
            char buf[1024];
            while(true)
            {
                int n = SSL_read(ssl, buf, 1024);
                if (n <= 0) {
                    print_ssl_error("SSL_read", ssl, n);
                    break;
                }

                buf[n] = 0;
                std::cout << buf << std::endl;

                n = SSL_write(ssl, buf, n);
                if (n <= 0) {
                    print_ssl_error("SSL_write", ssl, n);
                    break;
                }
            }
            SSL_free(ssl);
            close(sock);
        });
        th.detach();
    }
    SSL_CTX_free(ctx);
    close(server_socket);
    ERR_free_strings();
    
    return 0;
}