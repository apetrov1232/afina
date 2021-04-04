#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <string>
#include <deque>
#include <memory>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <spdlog/logger.h>
#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> lg, std::shared_ptr<Afina::Storage> st) : _socket(s), _logger(lg), storage(st){
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return isalive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;
    int _socket;
    struct epoll_event _event;
    char client_buffer[4096] = "";
    //std:: deque <std::string> client_buffer = {};
    int offset;
    bool isalive;
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> storage;
    std::deque<std::string> answer;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
