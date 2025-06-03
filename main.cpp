#include "serial.hpp"
#include "tcp.hpp"
#include "config.hpp"
#include <cstdint>
#include <thread>

void serial_read(SerialPort &serial, RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = serial.read(temp.data(), temp.size());
        if (amount_read > 0) ringBuffer.write(temp.data(), amount_read);
    }
}
void serial_proc(RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = ringBuffer.read(temp.data(), temp.size());
        // replace this, in prod, should write to app. buffer
        std::cout.write((char*)temp.data(), amount_read) << std::endl;
        std::cout.flush();
    }

}

void tcp_read(TcpSocket &socket, RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = socket.read(temp.data(), temp.size());
        if(amount_read > 0) ringBuffer.write(temp.data(), amount_read);
    }

}
void tcp_proc(RingBuffer &ringBuffer){
    std::vector<uint8_t> temp(READ_CHUNK);
    while(true){
        size_t amount_read = ringBuffer.read(temp.data(), temp.size());
        //replace this in prod, should write to app. buffer
        std::cout.write((char*)temp.data(), amount_read) << std::endl;
        std::cout.flush();
    }
}

//#define SERIAL
// well
int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    std::string portName = PORT;
    unsigned baud = 115200;
    std::string serverIP = IP;
    unsigned port = 5700;

    SerialPort serial(portName, baud);
    TcpSocket  tcp(serverIP, port);
    RingBuffer ringBuffer;

    
#ifdef SERIAL
    std::thread t_serial_read(serial_read, std::ref(serial), std::ref(ringBuffer));
    std::thread t_serial_proc(serial_proc, std::ref(ringBuffer));
    std::cin.get(); // closes on input
    t_serial_read.join();
    t_serial_proc.join();
#else
    std::thread t_tcp_read(tcp_read, std::ref(tcp), std::ref(ringBuffer));
    std::thread t_tcp_proc(tcp_proc, std::ref(ringBuffer));
    std::cin.get();
#endif
}
