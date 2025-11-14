#include "ringbuffer.hpp"

#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <ratio>
#include <stdexcept>
#include <system_error>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr unsigned CLIENT_PORT = 8187;
constexpr unsigned HOST_PORT = 5600;
constexpr unsigned TEST_PORT = 5500;

namespace {

int client_listen_fd = -1;

int create_listener(unsigned port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::system_error(errno, std::system_category(), "socket");
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::system_error(errno, std::system_category(), "bind");
    }

    if (::listen(fd, 32) < 0) {
        ::close(fd);
        throw std::system_error(errno, std::system_category(), "listen");
    }

    return fd;
}

void client_writer(int fd, RingBuffer& ring_buffer) {
    try {
        auto reader = ring_buffer.create_reader();
        std::vector<uint8_t> buffer(READ_CHUNK);

        while (ring_buffer.is_running()) {
            std::size_t available = reader.read_blocking(buffer.data(), buffer.size());
            if (available == 0) {
                if (!ring_buffer.is_running()) {
                    break;
                }
                continue;
            }

            std::size_t sent = 0;
            while (sent < available) {
                ssize_t wrote = ::send(fd, buffer.data() + sent, available - sent, MSG_NOSIGNAL);
                if (wrote <= 0) {
                    if (wrote < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    throw std::runtime_error("client disconnected");
                }
                sent += static_cast<std::size_t>(wrote);
            }
        }
    } catch (...) {
        // Fall through to close socket.
    }

    ::close(fd);
}

void client_accept_thread(RingBuffer& ring_buffer) {
    client_listen_fd = create_listener(CLIENT_PORT);

    while (true) {
        int fd = ::accept(client_listen_fd, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::thread(client_writer, fd, std::ref(ring_buffer)).detach();
    }

    // Listener remains open until process is terminated externally.
}

constexpr std::array<uint16_t, 8> kHeartbeatCanIds = {0x7FF, 0x002, 0x003, 0x1FF,
                                                      0x512, 0x400, 0x300, 0x114};
constexpr std::size_t kPayloadLength = 1;
constexpr std::size_t kFrameLength = 1 + 3 + 1 + (kPayloadLength * 2) + 1;

char hex_char(uint8_t value) {
    value &= 0xF;
    return value < 10 ? static_cast<char>('0' + value)
                      : static_cast<char>('A' + (value - 10));
}

constexpr std::array<uint8_t, 100> kSineTable = {
    128, 136, 143, 151, 159, 167, 174, 182, 189, 196, 202, 209, 215, 220,
    226, 231, 235, 239, 243, 246, 249, 251, 253, 254, 255, 255, 255, 254,
    253, 251, 249, 246, 243, 239, 235, 231, 226, 220, 215, 209, 202, 196,
    189, 182, 174, 167, 159, 151, 143, 136, 128, 119, 112, 104, 96, 88, 81,
    73, 66, 59, 53, 46, 40, 35, 29, 24, 20, 16, 12, 9, 6, 4, 2, 1, 0, 0, 0,
    1, 2, 4, 6, 9, 12, 16, 20, 24, 29, 35, 40, 46, 53, 59, 66, 73, 81, 88,
    96, 104, 112, 119};

void encode_frame(uint16_t can_id,
                  const std::array<uint8_t, kPayloadLength>& payload,
                  std::array<char, kFrameLength>& frame) {
    frame[0] = 't';
    frame[1] = hex_char((can_id >> 8) & 0xF);
    frame[2] = hex_char((can_id >> 4) & 0xF);
    frame[3] = hex_char(can_id & 0xF);
    frame[4] = static_cast<char>('0' + kPayloadLength);

    std::size_t cursor = 5;
    for (std::size_t i = 0; i < kPayloadLength; ++i) {
        frame[cursor++] = hex_char(payload[i] >> 4);
        frame[cursor++] = hex_char(payload[i]);
    }
    frame[cursor] = '\r';
}

void heartbeat(RingBuffer& ring_buffer) {
    std::array<char, kFrameLength> frame{};
    std::array<uint8_t, kPayloadLength> payload{};
    std::size_t index = 0;

    while (ring_buffer.is_running()) {
        payload[0] = kSineTable[index];

        for (uint16_t can_id : kHeartbeatCanIds) {
            encode_frame(can_id, payload, frame);
            ring_buffer.write(reinterpret_cast<const uint8_t*>(frame.data()),
                              frame.size());
        }

        index = (index + 1) % kSineTable.size();

	std::this_thread::sleep_for(std::chrono::milliseconds(125));
        std::this_thread::yield();
    }
}

// what do we need?
// a thread that just listens for new tcp connections on HOST_PORT
// if it receives a new connection, kill the existing pull and push thread
// create a new pull and push thread, connected to that port
// p&p will now:
// if the thread has not been killed:
// read from the tcp port
// push the contents to the ring buffer
int host_fd= -1;
std::atomic<bool> newHost = false;

void car_reader(int fd, RingBuffer& ring_buffer){
    try{
    std::array<uint8_t, READ_CHUNK> buffer;
    while(!newHost.load(std::memory_order_relaxed)){
        size_t readSize = ::read(fd, buffer.data(), buffer.size());
        if(readSize > 0){
            ring_buffer.write(buffer.data(), buffer.size());
        }
    }
    }catch(...){}
}

void car_accept_thread(RingBuffer& ring_buffer){
    try{
    std::thread currentInstance;
    host_fd= create_listener(TEST_PORT);
    while(1){
        int ret = ::accept(host_fd, nullptr, nullptr);
        if(ret < 0){
            if(ret == EINTR) continue;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        newHost = true;
        if(currentInstance.joinable()) currentInstance.join();
        newHost = false;
        currentInstance = std::thread(car_reader, ret, std::ref(ring_buffer));
    }
    }catch(...){}
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    RingBuffer ring_buffer;

    std::thread accept_thread(client_accept_thread, std::ref(ring_buffer));
    accept_thread.detach();

    std::thread car_thread(car_accept_thread, std::ref(ring_buffer));
    car_thread.detach();

    //std::thread heartbeat_thread(heartbeat, std::ref(ring_buffer));
    //heartbeat_thread.detach();

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}
