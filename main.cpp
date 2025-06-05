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
        std::cout << "Client Loop" << std::endl;
        read_amount = ringBuffer.read(temp.data(), temp.size());
        write_amount = client_socket.write(temp.data(), read_amount);
        if(write_amount <= 0){
            std::cout << "Client Reconnect Enter " << std::endl;
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
        std::cout << "Source Loop" << std::endl;
        // our source get's stuck here, we did not cleanly disc
        // so it gets stuck here as it is blocking
        // we are able to connect to it, but we are never re-accepted
        read_amount = source_socket.read(temp.data(), temp.size());
        if(read_amount <= 0){
            std::cout << "Source Reconnect Enter " << std::endl;
            source_socket.reconnect();
        } else {
            ringBuffer.write(temp.data(), read_amount);
        }
    }
}

// four threads
// one thread for handling client connections
// one thread for handling client rb.read -> tcp.write

// one thread for handling source connections
// one thread for handling source tcp.read -> rb.write

// at this point in time, we will only have 1 client and 1 source, however, this should also support N clients N sources

int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    RingBuffer ringBuffer;

    std::thread t_source_t(source_t, std::ref(ringBuffer));
    std::thread t_client_t(client_t, std::ref(ringBuffer));
    std::cin.get();
    t_source_t.join();
    t_client_t.join();
}
