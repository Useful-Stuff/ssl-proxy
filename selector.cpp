#include "selector.h"

void Selector::addEvent(int sock, int event, EventHandler h)
{
    auto [iter, inserted] = m_handlers.emplace(sock, h);
    if (!inserted) {
        throw 28;
    }
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = event;
    pfd.revents = 0;
    m_pfds.push_back(pfd);  
}


void Selector::addReadEvent(int sock, EventHandler h) {    
    addEvent(sock, POLLIN, h);
}


void Selector::addWriteEvent(int sock, EventHandler h) {
    addEvent(sock, POLLOUT, h);
}


int Selector::run()
{
    m_stop.store(false);
    while (!m_stop.load()) 
    {
        int ready_n = poll(&m_pfds[0], m_pfds.size(), TIMEOUT_MS);

        if (ready_n == -1) {
            perror("poll");
            return errno;
        }
        
        if (ready_n == 0) {
            continue;
        }
        executeHandlers();
    }
    return 0;
}


void Selector::executeHandlers()
{
    auto pfds = std::move(m_pfds);
    auto handlers = std::move(m_handlers);

    for (auto& pfd : pfds) 
    {
        if (pfd.revents & (POLLIN | POLLOUT | POLLERR | POLLHUP)) {
            handlers[pfd.fd](pfd.fd);
            continue;
        }
        addEvent(pfd.fd, pfd.events, handlers[pfd.fd]);
    }
}


void Selector::stop() {
    m_stop.store(true);
}