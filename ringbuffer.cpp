#include "ringbuffer.hpp"

#include <algorithm>
#include <cstring>

RingBuffer::Reader::Reader() noexcept : owner(nullptr), slot(0), state(nullptr) {}

RingBuffer::Reader::Reader(RingBuffer* owner, std::size_t slot, ConsumerState* state)
    : owner(owner), slot(slot), state(state) {}

RingBuffer::Reader::Reader(Reader&& other) noexcept
    : owner(other.owner), slot(other.slot), state(other.state) {
    other.owner = nullptr;
    other.state = nullptr;
}

RingBuffer::Reader& RingBuffer::Reader::operator=(Reader&& other) noexcept {
    if (this != &other) {
        release();
        owner = other.owner;
        slot = other.slot;
        state = other.state;
        other.owner = nullptr;
        other.state = nullptr;
    }
    return *this;
}

RingBuffer::Reader::~Reader() { release(); }

void RingBuffer::Reader::release() {
    if (owner) {
        owner->release_slot(slot);
        owner = nullptr;
        state = nullptr;
    }
}

bool RingBuffer::Reader::valid() const noexcept {
    return owner != nullptr && state != nullptr;
}

size_t RingBuffer::Reader::read(uint8_t* out, size_t maxlen) {
    if (!valid() || maxlen == 0) {
        return 0;
    }
    size_t tail = state->read_index.load(std::memory_order_acquire);
    size_t head = owner->write_index.load(std::memory_order_acquire);
    if (tail >= head) {
        return 0;
    }
    size_t available = head - tail;
    size_t to_read = std::min(maxlen, available);
    owner->copy_out(tail, out, to_read);
    state->read_index.store(tail + to_read, std::memory_order_release);
    return to_read;
}

size_t RingBuffer::Reader::read_blocking(uint8_t* out, size_t maxlen) {
    if (!valid()) {
        return 0;
    }

    size_t bytes = read(out, maxlen);
    while (bytes == 0 && owner && owner->is_running()) {
        owner->wait_for_data(state);
        bytes = read(out, maxlen);
    }

    if (!owner || !owner->is_running()) {
        bytes = read(out, maxlen);
    }
    return bytes;
}

RingBuffer::RingBuffer() : write_index(0), running(true) {}

RingBuffer::~RingBuffer() { close(); }

void RingBuffer::write(const uint8_t* data, size_t len) {
    if (!data || len == 0 || !running.load(std::memory_order_acquire)) {
        return;
    }

    if (len >= BUFFERSIZE) {
        data += len - (BUFFERSIZE - 1);
        len = BUFFERSIZE - 1;
    }

    size_t head = write_index.load(std::memory_order_relaxed);
    size_t written = 0;

    while (written < len && running.load(std::memory_order_acquire)) {
        size_t min_read = compute_min_read(head);
        size_t used = head - min_read;
        if (used >= BUFFERSIZE) {
            advance_slow_consumers(head - BUFFERSIZE + 1);
            continue;
        }

        size_t free_space = BUFFERSIZE - used;
        size_t chunk = std::min(len - written, free_space);
        copy_into(head, data + written, chunk);
        head += chunk;
        written += chunk;
        write_index.store(head, std::memory_order_release);
        data_available.notify_all();
    }
}

RingBuffer::Reader RingBuffer::create_reader() {
    std::lock_guard<std::mutex> lock(registration_mtx);
    for (std::size_t i = 0; i < kMaxConsumers; ++i) {
        bool expected = false;
        if (consumers[i].active.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            std::size_t head = write_index.load(std::memory_order_acquire);
            consumers[i].read_index.store(head, std::memory_order_release);
            return Reader(this, i, &consumers[i]);
        }
    }
    throw std::runtime_error("no free consumer slots available");
}

bool RingBuffer::is_running() const noexcept {
    return running.load(std::memory_order_acquire);
}

void RingBuffer::close() {
    bool was_running = running.exchange(false, std::memory_order_acq_rel);
    if (was_running) {
        data_available.notify_all();
    }
}

std::size_t RingBuffer::compute_min_read(std::size_t fallback) const {
    std::size_t min_value = fallback;
    for (const auto& consumer : consumers) {
        if (!consumer.active.load(std::memory_order_acquire)) {
            continue;
        }
        std::size_t value = consumer.read_index.load(std::memory_order_acquire);
        if (value < min_value) {
            min_value = value;
        }
    }
    return min_value;
}

void RingBuffer::advance_slow_consumers(std::size_t minimum) {
    for (auto& consumer : consumers) {
        if (!consumer.active.load(std::memory_order_acquire)) {
            continue;
        }
        std::size_t current = consumer.read_index.load(std::memory_order_acquire);
        if (current < minimum) {
            consumer.read_index.store(minimum, std::memory_order_release);
        }
    }
}

void RingBuffer::release_slot(std::size_t slot) {
    if (slot >= kMaxConsumers) {
        return;
    }
    auto& consumer = consumers[slot];
    bool was_active = consumer.active.exchange(false, std::memory_order_acq_rel);
    if (was_active) {
        data_available.notify_all();
    }
}

void RingBuffer::copy_into(std::size_t index, const uint8_t* data, std::size_t len) {
    if (len == 0) {
        return;
    }
    std::size_t pos = index % BUFFERSIZE;
    std::size_t first = std::min(len, BUFFERSIZE - pos);
    std::memcpy(buffer.data() + pos, data, first);
    if (len > first) {
        std::memcpy(buffer.data(), data + first, len - first);
    }
}

void RingBuffer::copy_out(std::size_t index, uint8_t* data, std::size_t len) const {
    if (len == 0) {
        return;
    }
    std::size_t pos = index % BUFFERSIZE;
    std::size_t first = std::min(len, BUFFERSIZE - pos);
    std::memcpy(data, buffer.data() + pos, first);
    if (len > first) {
        std::memcpy(data + first, buffer.data(), len - first);
    }
}

void RingBuffer::wait_for_data(Reader::ConsumerState* state) {
    std::unique_lock<std::mutex> lock(wait_mtx);
    data_available.wait(lock, [&] {
        return !running.load(std::memory_order_acquire) ||
               state->read_index.load(std::memory_order_acquire) <
                   write_index.load(std::memory_order_acquire);
    });
}
