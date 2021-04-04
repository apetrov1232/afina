#include "Connection.h"

#include <iostream>
#include <cassert>
#include <unistd.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() { 
    isalive = true;
    offset = 0;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
}

// See Connection.h
void Connection::OnError() { 
    isalive = false;
    _event.events = 0;
}

// See Connection.h
void Connection::OnClose() {
    isalive = false;
    _event.events = 0;
}

// See Connection.h
void Connection::DoRead() { 
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    try {
        std::size_t parsed;
        int readed_bytes = -1;
        //char client_buffer[4096] = "";
        if ((readed_bytes = read(_socket, client_buffer + offset, sizeof(client_buffer)-offset)) > 0) {
           _logger->debug("Got {} bytes from socket", readed_bytes);
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                //_logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    parsed = 0;
                    if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        //_logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }
                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        offset += parsed;
                        readed_bytes -= parsed;
                    }   
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer, to_read);
                    arg_remains -= to_read;
                    readed_bytes -= parsed;
                    offset += to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    if (argument_for_command.size()) {
                        argument_for_command.resize(argument_for_command.size() - 2);
                    }
                    command_to_execute->Execute(*storage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    answer.push_back(std::move(result));

                    if (answer.size() > 300) {
                        _event.events &= ~EPOLLIN;
                    }

                    if (!(_event.events & EPOLLOUT)) {
                        _event.events |= EPOLLOUT;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }
        if (readed_bytes == 0) {
            isalive = false;
            _logger->debug("Connection closed");
        } else {
            isalive = false;
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
    iovec vec[100] = {};
    auto elem = answer.begin();
    vec[0].iov_base = &((*elem)[0]) + offset;
    vec[0].iov_len = elem->size() - offset;
    elem++;
    int num = 1;
    for (elem; elem != answer.end(); elem++){
        vec[num].iov_base = &((*elem)[0]);
        vec[num].iov_len = elem->size();
        num++;
        if (num >= 100){
            break;
        }
    }
    int n = 0;
    if ((n = writev(_socket, vec, num)) > 0) {
        int i = 0;
        while ((i < num) && (n >= vec[i].iov_len)) {
            answer.pop_front();
            n -= vec[i].iov_len;
            i++;
        }
        offset = n;
    } else if (n < 0 && n != EAGAIN) {
        isalive = false;
    }
    if (answer.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (answer.size() < 200){
        _event.events |= EPOLLIN;
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
