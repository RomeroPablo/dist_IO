#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <iostream>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "serial.hpp"

class TcpSocket {
    public:
        TcpSocket(const std::string& serverIP, unsigned port);
        ~TcpSocket();

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;
        ssize_t read(uint8_t* buf, std::size_t maxlen);
        ssize_t write(const uint8_t* buf, std::size_t len);
        void reconnect();

    private:
        int _fd = -1;
        int _listen = -1;

};
