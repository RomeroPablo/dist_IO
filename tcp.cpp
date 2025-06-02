#include "tcp.hpp"
#include <cstdint>
#include <sys/socket.h>
#include <system_error>
#include <termios.h>

TcpSocket::TcpSocket(const std::string& serverIP, unsigned port){
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if(_fd < 0)
        throw std::system_error(errno, std::system_category(), "socket creation failed");
    
    if(serverIP.size() != 0){
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr) <= 0)
        throw std::system_error(errno, std::system_category(), "pton failed");

    if(connect(_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        throw std::system_error(errno, std::system_category(), "connection failed");
    }
    else{
    int opt = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    sockaddr_in client_addr = {};
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(port);

    if(bind(_fd, (sockaddr*)&client_addr, sizeof(client_addr)) < 0)
        throw std::system_error(errno, std::system_category(), "bind failed");
    listen(_fd, 16);
    while(accept(_fd, nullptr, nullptr) < 0){
        sleep(1);
    }
    }
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
    while(accept(_fd, nullptr, nullptr) < 0){
        std::cout << "attempting reconnect" << std::endl;
        sleep(1);
    }
}
