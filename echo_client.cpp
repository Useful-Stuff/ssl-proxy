#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
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


int createTCPConnection(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr))) {
        perror("connect");
        return -1;
    }

    return sock;
}


void makeEcho(int sock, int i)
{
    char buf[1024];
    sprintf(buf, "%d: Hello! [%d]", sock, i);

    int n = write(sock, buf, strlen(buf));
    if (n < 0) {
        perror("write");
        return;
    }

    memset(buf, 0, strlen(buf));

    n = read(sock, buf, 1024);
    if (n < 0) {
        perror("read");
        return;
    }

    println(buf);
}


int main()
{
    std::atomic<bool> wait = true;

    std::thread *threads[10];

    for (int i = 0; i < 10; ++i)
    {
        int sock = createTCPConnection(8080);
        std::thread *th = new std::thread([sock, &wait]() 
        {
            while(wait.load());

            for (int i = 0; i < 5; ++i) {
                makeEcho(sock, i);
            }
            close(sock);
        });
        threads[i] = th;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    wait.store(false);
    
    for (std::thread* th : threads) {
        th->join();
        delete th;
    }

    return 0;
}