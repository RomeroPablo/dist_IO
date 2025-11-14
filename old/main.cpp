#include "ringbuffer.hpp"
#include <cerrno>
#include <cstdint>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <cstring>

constexpr unsigned CLIENT_PORT = 8187;
constexpr unsigned SOURCE_PORT = 5600;

static std::vector<int> clients;
static std::mutex clients_mtx;

static int source_fd = -1;
static int source_listen_fd = -1;
static std::mutex source_mtx;
static std::condition_variable source_cv;

int create_listener(unsigned port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) throw std::system_error(errno, std::system_category(), "socket");
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if(bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::system_error(errno, std::system_category(), "bind");
    if(listen(fd, 16) < 0)
        throw std::system_error(errno, std::system_category(), "listen");
    return fd;
}

void source_connection_thread(){
    source_listen_fd = create_listener(SOURCE_PORT);
    while(true){
        int fd = accept(source_listen_fd, nullptr, nullptr);
        if(fd < 0) continue;
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        {
            std::lock_guard<std::mutex> lock(source_mtx);
            if(source_fd >= 0) close(source_fd);
            source_fd = fd;
        }
        source_cv.notify_all();
//        std::cout << "[+] Source connected" << std::endl;
    }
}

void source_read_thread(RingBuffer &rb){
    std::vector<uint8_t> buf(READ_CHUNK);
    //std::string meta_s = "t78A101\r";
    //std::vector<uint8_t> meta(meta_s.begin(), meta_s.end());

    while(true){
        int fd = -1;
	ssize_t n = 0;
        {
            std::unique_lock<std::mutex> lock(source_mtx);
            //source_cv.wait(lock, []{return source_fd >= 0;});
            fd = source_fd;
        }
	if(fd > 0)
        	n = ::recv(fd, buf.data(), buf.size(), 0);
        if(n <= 0){
            if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                continue;
            {
                std::lock_guard<std::mutex> lock(source_mtx);
                if(source_fd >= 0) close(source_fd);
                source_fd = -1;
            }
//            std::cout << "[!] Source disconnected" << std::endl;
            continue;
        }
	//buf.insert(buf.end(), meta.begin(), meta.end());
        rb.write(buf.data(), n);
    }
}

void client_accept_thread(){
    int listen_fd = create_listener(CLIENT_PORT);
    std::string avail = "avail";
    while(true){
        int fd = accept(listen_fd, nullptr, nullptr);
        if(fd < 0) continue;
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	char token[6];
	ssize_t got = recv(fd, token, sizeof(token), 0);
	if(got != sizeof(token) || memcmp(token, "xmN10e", sizeof(token)) != 0){
		close(fd);
		continue;
	}

	char rec_ver [5];
	ssize_t ver_len = recv(fd, rec_ver, sizeof(rec_ver), 0);
	if(memcmp(rec_ver, "1.0.3", sizeof(rec_ver)) != 0){
		::send(fd, "1.0.3", 5, MSG_NOSIGNAL);
	}

        {
            std::lock_guard<std::mutex> lock(clients_mtx);
            clients.push_back(fd);
        }
//        std::cout << "[+] Client connected" << std::endl;
    }
}

void writer_thread(RingBuffer &rb){
    std::vector<uint8_t> buf(READ_CHUNK );
    while(true){
        size_t n = rb.read(buf.data(), buf.size());
        std::lock_guard<std::mutex> lock(clients_mtx);
        for(auto it = clients.begin(); it != clients.end(); ){
            ssize_t w = ::send(*it, buf.data(), n, MSG_NOSIGNAL);
            if(w <= 0){
//                std::cout << "[!] Removing client" << std::endl;
                close(*it);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void heartbeat(RingBuffer &rb){
	const char msg_on[] = "t78A101\r";
	const char msg_off[] = "t78A101\r";
	bool state = false;

	std::string message = "hello from the other side!";

	while(true){
		/*
		if(!state){
			rb.write(reinterpret_cast<const uint8_t*>(msg_off), sizeof(msg_off)-1);
		} else {
			rb.write(reinterpret_cast<const uint8_t*>(msg_on), sizeof(msg_on)-1);
		}
		state = !state;
		*/
		rb.write(reinterpret_cast<const uint8_t*>(message.c_str()), message.size());
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

int main(int argc, char* argv[]){
    (void)argc; (void)argv;
    RingBuffer ringBuffer;

    std::thread t_conn(source_connection_thread);
    std::thread t_read(source_read_thread, std::ref(ringBuffer));
    std::thread t_client(client_accept_thread);
    std::thread t_writer(writer_thread, std::ref(ringBuffer));
    std::thread t_heart(heartbeat, std::ref(ringBuffer));

    std::cin.get();

    t_conn.join();
    t_read.join();
    t_client.join();
    t_writer.join();
    t_heart.join();
    return 0;
}

// four threads:
// source connection thread - e.g. connecting to the lte module
// source write to buffer thread - e.g. forward lte data to the Ring Buffer
// client connection thread - e.g. deal with clients connecting and disconnecting from the server
// client read thread - e.g. forwarding buffer contents to all the clients

