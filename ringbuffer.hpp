#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>

constexpr size_t BUFFERSIZE = 64 * 1024;
constexpr size_t READ_CHUNK = 4096;

class RingBuffer {
public:
    class Reader {
    public:
        Reader() noexcept;
        Reader(Reader&& other) noexcept;
        Reader& operator=(Reader&& other) noexcept;
        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;
        ~Reader();

        size_t read(uint8_t* out, size_t maxlen);
        size_t read_blocking(uint8_t* out, size_t maxlen);
        bool valid() const noexcept;

    private:
        friend class RingBuffer;
        struct ConsumerState;
        Reader(RingBuffer* owner, std::size_t slot, ConsumerState* state);
        void release();

        RingBuffer* owner;
        std::size_t slot;
        ConsumerState* state;
    };

    RingBuffer();
    ~RingBuffer();

    void write(const uint8_t* data, size_t len);
    Reader create_reader();
    bool is_running() const noexcept;
    void close();

private:
    friend class Reader;
    struct Reader::ConsumerState {
        std::atomic<bool> active{false};
        std::atomic<std::size_t> read_index{0};
    };

    static constexpr std::size_t kMaxConsumers = 64;

    std::size_t compute_min_read(std::size_t fallback) const;
    void advance_slow_consumers(std::size_t minimum);
    void release_slot(std::size_t slot);
    void copy_into(std::size_t index, const uint8_t* data, std::size_t len);
    void copy_out(std::size_t index, uint8_t* data, std::size_t len) const;
    void wait_for_data(Reader::ConsumerState* state);

    std::array<uint8_t, BUFFERSIZE> buffer{};
    std::array<Reader::ConsumerState, kMaxConsumers> consumers{};
    std::atomic<std::size_t> write_index;
    std::atomic<bool> running;
    std::mutex registration_mtx;
    std::condition_variable data_available;
    std::mutex wait_mtx;
};
