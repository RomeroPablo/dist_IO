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

#define CLIENT_PORT 5700
void client_t(RingBuffer &ringBuffer){
    TcpSocket client_socket("", CLIENT_PORT);
    std::vector<uint8_t> temp(READ_CHUNK);
    int read_amount = 0;
    int write_amount = 0;
    while(1){
        // read from rb, write to socket
        read_amount = ringBuffer.read(temp.data(), temp.size());
        write_amount = client_socket.write(temp.data(), read_amount);
        if(write_amount <= 0){
            client_socket.reconnect();
        }
    }
}

#define SOURCE_PORT 5600
void source_t(RingBuffer &ringBuffer){
    TcpSocket source_socket("", SOURCE_PORT);
    std::vector<uint8_t> temp(READ_CHUNK);
    int read_amount = 0;
    while(1){
        read_amount = source_socket.read(temp.data(), temp.size());
        if(read_amount <= 0){
            source_socket.reconnect();
        }
        ringBuffer.write(temp.data(), read_amount);
    }
}

int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    RingBuffer ringBuffer;

    std::thread t_source_t(source_t, std::ref(ringBuffer));
    std::thread t_client_t(client_t, std::ref(ringBuffer));
    std::cin.get();
    t_source_t.join();
    t_client_t.join();
}
