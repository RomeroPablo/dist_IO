#include "tcp.hpp"
#include <cstdint>
#include <sys/socket.h>
#include <system_error>
#include <termios.h>

TcpSocket::TcpSocket(const std::string& serverIP, unsigned port){
   if(serverIP.size() != 0){
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if(_fd < 0)
        throw std::system_error(errno, std::system_category(), "socket creation failed");

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr) <= 0)
        throw std::system_error(errno, std::system_category(), "pton failed");

    if(connect(_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        throw std::system_error(errno, std::system_category(), "connection failed");

    } else {
    _listen = socket(AF_INET, SOCK_STREAM, 0);
    if(_listen < 0)
        throw std::system_error(errno, std::system_category(), "socket creation failed");
    int opt = 1;
    setsockopt(_listen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if(bind(_listen, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::system_error(errno, std::system_category(), "bind failed");
    listen(_listen, 16);
    while( _fd < 0){
        _fd = accept(_listen, nullptr, nullptr);
        sleep(1);
    }
    }
   std::cout << "[+] Initialized TCP Socket: " << _fd << std::endl;
}

TcpSocket::~TcpSocket(){
    if(_fd >= 0) ::close(_fd);
}

std::size_t TcpSocket::read(uint8_t* buf, std::size_t maxlen){
    ssize_t n = ::recv(_fd, buf, maxlen, 0);
    return n;
}

std::size_t TcpSocket::write(const uint8_t* buf, std::size_t len){
    ssize_t n = send(_fd, buf, len, 0);
    return n;
}

void TcpSocket::reconnect(){
    while(accept(_fd, nullptr, nullptr) <= 0){
        std::cout << "[!] attempting reconnect fd: " << _fd << std::endl;
        sleep(1);
    }
}
