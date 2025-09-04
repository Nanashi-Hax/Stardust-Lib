#include <cstddef>
#include <cstdint>
#include <mutex>

class Socket
{
private:
    int socketFD = -1;
    std::mutex mtx;

public:
    enum class Result { Success, WouldBlock, Closed, Error };

    Socket() = default;
    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept
    {
        socketFD = other.socketFD;
        other.socketFD = -1;
    }

    Socket& operator=(Socket&& other) noexcept
    {
        if(this != &other)
        {
            close();
            socketFD = other.socketFD;
            other.socketFD = -1;
        }
        return *this;
    }

    Result create(bool nonBlocking = true, bool noDelay = true);

    Result bind(uint16_t port);
    Result listen(int backlog = 16);
    Result accept(Socket& outClient);

    Result send(const void* data, ssize_t size, ssize_t& outBytes);
    Result recv(void* buffer, ssize_t size, ssize_t& outBytes);
    Result close();
};