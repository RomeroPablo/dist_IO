#include "serial.hpp"
#include "tcp.hpp"
#include <cstdint>
#include <thread>

#define CLIENT_PORT 5700
void client_t(RingBuffer &ringBuffer){
    TcpSocket client_socket("", CLIENT_PORT);
    std::vector<uint8_t> temp(READ_CHUNK);
    int read_amount = 0;
    int write_amount = 0;
    while(true){
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
    while(true){
        read_amount = source_socket.read(temp.data(), temp.size());
        if(read_amount <= 0){
            //std::cout << "src rec" << std::endl;
            //source_socket.reconnect();
        }
        ringBuffer.write(temp.data(), read_amount);
    }
}

// at the moment:
// order of launch does not matter :)
// works, but problems:
// crashes when client closes connection
// infinite loop when source closes connection, but then won't crash when client closes after
// poor serial line output info
//
// if you connect your client and leave, nothing happens
// if you then connect your source, it will crash
// I believe your source thread is trying to write to something (or read from something!) which causes the program to crash

int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    RingBuffer ringBuffer;

    std::thread t_source_t(source_t, std::ref(ringBuffer));
    std::thread t_client_t(client_t, std::ref(ringBuffer));
    std::cin.get();
    t_source_t.join();
    t_client_t.join();
}
