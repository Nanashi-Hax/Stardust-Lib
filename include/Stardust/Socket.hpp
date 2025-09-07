#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <algorithm>

class Socket
{
private:
    int socketFd = -1;
    std::mutex mutex;

public:
    enum class Result { Success, WouldBlock, Closed, Error };

    Socket() : socketFd(-1) {}
    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    // Server
    Result create(bool nonBlocking = true, bool noDelay = true);
    Result bind(uint16_t port);
    Result listen(int backlog = 16);
    Result accept(std::unique_ptr<Socket>& outClient, uint32_t& outIPAddress);

    // Client
    Result send(const void* data, ssize_t size, ssize_t& outBytes);
    Result recv(void* buffer, ssize_t size, ssize_t& outBytes);

    // Common
    Result close();
    int getFd() const;
};