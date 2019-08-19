#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


std::mutex coutMutex;

template<typename T>
void println(T t)
{
    coutMutex.lock();
    std::cout << t << std::endl;
    coutMutex.unlock();
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

    if (listen(server_socket, 10) < 0) {
        perror("listen");
        return 3;
    }

    while(true) 
    {
        int sock = accept(server_socket, nullptr, nullptr);
        if(sock < 0) {
            perror("accept");
            break;
        }

        std::thread th([sock]() 
        {
            char buf[1024];
            while(true)
            {
                int n = read(sock, buf, 1024);
                if (n <= 0) {
                    perror("read");
                    break;
                }

                buf[n] = 0;
                println(buf);

                n = write(sock, buf, n);
                if (n <= 0) {
                    perror("write");
                    break;
                }
            }
            close(sock);
        });
        th.detach();
    }
    close(server_socket);
    
    return 0;
}