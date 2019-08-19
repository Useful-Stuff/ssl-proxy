#ifndef SELECTOR_H
#define SELECTOR_H

#include <functional>
#include <map>
#include <vector>
#include <atomic>

#include <poll.h>


using EventHandler = std::function<void (int sock)>;

class Selector
{
    static const int TIMEOUT_MS = 50;

    std::map<int, EventHandler> m_handlers;
    std::vector<struct pollfd>  m_pfds;
    std::atomic<bool>           m_stop;

public:
    void addReadEvent(int sock, EventHandler h);
    void addWriteEvent(int sock, EventHandler h);

    int run();
    void stop();

private:
    void addEvent(int sock, int events, EventHandler h);
    void executeHandlers();
};

#endif // SELECTOR_H
