#include "serial.hpp"
#include "tcp.hpp"
#include "ringbuffer.hpp"
#include "candb.hpp"
#include "config.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>
#include <array>
#include <memory>

static CanStore can_store;

enum class ParseState : uint8_t {
    WaitStart,
    Id,
    Len,
    DataHigh,
    DataLow,
    End
};

static inline uint8_t hex_value(uint8_t c){
    static const std::array<int8_t, 256> table = []{
        std::array<int8_t, 256> t{};
        t.fill(-1);
        for(uint8_t i = 0; i < 10; ++i)
            t['0' + i] = i;
        for(uint8_t i = 0; i < 6; ++i){
            t['A' + i] = 10 + i;
            t['a' + i] = 10 + i;
        }
        return t;
    }();
    return table[c];
}

inline void dispatch(uint32_t id, uint8_t len, const uint8_t* payload){
   can_store.store(static_cast<CanStore::IdType>(id), len, payload);
   // TODO remove debug output b4 deployment
    std::cout << "ID:" << std::hex << id << " LEN:" << std::dec
              << static_cast<int>(len) << " DATA:";
    for(uint8_t i = 0; i < len; ++i)
        std::cout << std::hex << static_cast<int>(payload[i]);
    std::cout << std::dec << std::endl;
}

void parse(const uint8_t* data, size_t len){
    static ParseState state = ParseState::WaitStart;
    static uint32_t id = 0;
    static uint8_t id_digits = 0;
    static uint8_t dlen = 0;
    static uint8_t payload[8];
    static uint8_t index = 0;

    for(size_t i = 0; i < len; ++i){
        uint8_t c = data[i];
        switch(state){
            case ParseState::WaitStart:
                if(c == 't'){
                    id = 0;
                    id_digits = 0;
                    state = ParseState::Id;
                }
                break;
            case ParseState::Id: {
                uint8_t v = hex_value(c);
                if(v < 16){
                    id = (id << 4) | v;
                    if(++id_digits == 3)
                        state = ParseState::Len;
                }else{
                    state = ParseState::WaitStart;
                }
                break;
            }
            case ParseState::Len: {
                uint8_t v = hex_value(c);
                if(v < 16){
                    dlen = v;
                    index = 0;
                    state = dlen ? ParseState::DataHigh : ParseState::End;
                }else{
                    state = ParseState::WaitStart;
                }
                break;
            }
            case ParseState::DataHigh: {
                uint8_t v = hex_value(c);
                if(v < 16){
                    payload[index] = v << 4;
                    state = ParseState::DataLow;
                }else{
                    state = ParseState::WaitStart;
                }
                break;
            }
            case ParseState::DataLow: {
                uint8_t v = hex_value(c);
                if(v < 16){
                    payload[index] |= v;
                    if(++index == dlen)
                        state = ParseState::End;
                    else
                        state = ParseState::DataHigh;
                }else{
                    state = ParseState::WaitStart;
                }
                break;
            }
            case ParseState::End:
                if(c == '\r')
                    dispatch(id, dlen, payload);
                state = ParseState::WaitStart;
                break;
        }
    }
}

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
        //std::cout.write((char*)temp.data(), amount_read) << std::endl;
        //std::cout.flush();
        parse((uint8_t*)temp.data(), amount_read);
    }
}

enum source_t {
    local,
    remote
};
int main(int argc, char* argv[]){
    (void)argc;(void)argv;
    int res = 0;
    if (argc == 0)
        res = std::stoi(argv[1]);
    source_t source = (source_t)res;

    std::string portName = "/dev/pts/3";//PORT;
    unsigned baud = 115200;
    std::string serverIP = IP;
    unsigned port = 5700;

    std::unique_ptr<SerialPort> serial;
    std::unique_ptr<TcpSocket> tcp;

    RingBuffer ringBuffer;

    std::thread prod_t;

    if(source == local){
        serial = std::make_unique<SerialPort>(portName, baud);
        prod_t = std::thread(serial_read, std::ref(*serial), std::ref(ringBuffer));
    }

    if(source == remote){
        tcp = std::make_unique<TcpSocket>(serverIP, port);
        prod_t = std::thread(tcp_read, std::ref(*tcp), std::ref(ringBuffer));
    }

    std::thread photon_t(photon_proc, std::ref(ringBuffer));

    std::cin.get();
    prod_t.join();
    photon_t.join();
}
