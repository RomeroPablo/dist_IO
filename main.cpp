#include "serial.hpp"
#include "tcp.hpp"
#include "config.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>

void serial_read(SerialPort &serial, RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = serial.read(temp.data(), temp.size());
        if (amount_read > 0) ringBuffer.write(temp.data(), amount_read);
    }
}
void tcp_read(TcpSocket &socket, RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = socket.read(temp.data(), temp.size());
        if(amount_read > 0) ringBuffer.write(temp.data(), amount_read);
    }

}

void photon_proc(RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = ringBuffer.read(temp.data(), temp.size());
        //replace this in prod, should write to app. buffer
        std::cout.write((char*)temp.data(), amount_read) << std::endl;
        std::cout.flush();
    }
}

// have two threads, and just wake depending on whatev
// think about the synch later ig
enum source_t {
    local,
    remote
};
int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    int res = std::stoi(argv[1]);
    source_t source = (source_t)res;

    std::string portName = PORT;
    unsigned baud = 115200;
    std::string serverIP = IP;
    unsigned port = 5700;

    SerialPort serial(portName, baud);
    TcpSocket  tcp(serverIP, port);

    RingBuffer ringBuffer;

    std::thread prod_t;

    if(source == local)
         prod_t = std::thread(serial_read, std::ref(serial), std::ref(ringBuffer));

    if(source == remote)
        prod_t = std::thread(tcp_read, std::ref(tcp), std::ref(ringBuffer));

    std::thread photon_t(photon_proc, std::ref(ringBuffer));

    std::cin.get();
    prod_t.join();
    photon_t.join();
}
