#include "tcp.hpp"
#include <cstdint>
#include <sys/socket.h>
#include <system_error>

TcpSocket::TcpSocket(const std::string& serverIP, unsigned port){
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
}

TcpSocket::~TcpSocket(){
    if(_fd >= 0) ::close(_fd);
}

std::size_t TcpSocket::read(uint8_t* buf, std::size_t maxlen){
    ssize_t n = ::recv(_fd, buf, maxlen, 0);
    return n;
}

void TcpSocket::write(const uint8_t* buf, std::size_t len){
    [[maybe_unused]] ssize_t n = ::write(_fd, buf, len);
}
