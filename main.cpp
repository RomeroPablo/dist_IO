#include "serial.hpp"
#include "tcp.hpp"
#include <cstdint>
#include <thread>

#define CLIENT_PORT 5700
void client_t(RingBuffer &ringBuffer){
    TcpSocket client_socket("", CLIENT_PORT);
    std::cout << "client connected" << std::endl;
    std::vector<uint8_t> temp(READ_CHUNK);
    int read_amount = 0;
    int write_amount = 0;
    while(1){
        std::cout << "cltA" << std::endl;
        read_amount = ringBuffer.read(temp.data(), temp.size());
        write_amount = client_socket.write(temp.data(), read_amount);
        std::cout << "wrote to client socket" << std::endl;
        if(write_amount <= 0){
            client_socket.reconnect();
        }
    }
}

#define SOURCE_PORT 5600
void source_t(RingBuffer &ringBuffer){
    TcpSocket source_socket("", SOURCE_PORT);
    std::cout << "source connected" << std::endl;
    std::vector<uint8_t> temp(READ_CHUNK);
    int read_amount = 0;
    while(1){
        std::cout << "srcA" << std::endl;
        read_amount = source_socket.read(temp.data(), temp.size());
        if(read_amount <= 0){
            source_socket.reconnect();
        }
        ringBuffer.write(temp.data(), read_amount);
        std::cout << "source wrote to buffer" << std::endl;
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
