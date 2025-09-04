#include <Stardust/Socket.hpp>

#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

Socket::Result Socket::create(bool nonBlocking, bool noDelay)
{
    socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socketFD < 0)
    {
        close();
        return Result::Error;
    }

    int ret;

    int opt = 1;
    ret = setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret < 0)
    {
        close();
        return Result::Error;
    }

    if(noDelay)
    {
        ret = setsockopt(socketFD, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if(ret < 0)
        {
            close();
            return Result::Error;
        }
    }

    if(nonBlocking)
    {
        int flags = fcntl(socketFD, F_GETFL, 0);
        if(flags < 0)
        {
            close();
            return Result::Error;
        }
        ret = fcntl(socketFD, F_SETFL, flags | O_NONBLOCK);
        if(ret < 0)
        {
            close();
            return Result::Error;
        }
    }

    return Result::Success;
}

Socket::Result Socket::bind(uint16_t port)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(::bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close();
        return Result::Error;
    }

    return Result::Success;
}

Socket::Result Socket::listen(int backlog)
{
    if(::listen(socketFD, backlog) < 0)
    {
        close();
        return Result::Error;
    }

    return Result::Success;
}

Socket::Result Socket::accept(Socket& outClient)
{
    int clientFD = ::accept(socketFD, nullptr, nullptr);
    if(clientFD >= 0)
    {
        outClient.socketFD = clientFD;
        return Result::Success;
    }
    else
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return Result::WouldBlock;
        }
        return Result::Error;
    }
}

Socket::Result Socket::send(const void* data, ssize_t size, ssize_t& outBytes)
{
    if(socketFD < 0) return Result::Error;

    std::lock_guard<std::mutex> lock(mtx);
    outBytes = 0;
    const uint8_t* ptr = static_cast<const uint8_t*>(data);

    while(outBytes < size)
    {
        ssize_t sent = ::send(socketFD, ptr + outBytes, size - outBytes, 0);
        if(sent > 0)
        {
            outBytes += sent;
        }
        else if(sent == 0)
        {
            return Result::Closed;
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return Result::WouldBlock;
            }
            return Result::Error;
        }
    }
    return Result::Success;
}

Socket::Result Socket::recv(void* buffer, ssize_t size, ssize_t& outBytes)
{
    if(socketFD < 0) return Result::Error;

    std::lock_guard<std::mutex> lock(mtx);
    outBytes = 0;
    uint8_t* ptr = static_cast<uint8_t*>(buffer);

    while(outBytes < size)
    {
        ssize_t recvd = ::recv(socketFD, ptr + outBytes, size - outBytes, 0);
        if(recvd > 0)
        {
            outBytes += recvd;
        }
        else if(recvd == 0)
        {
            return Result::Closed;
        }
        else if(recvd < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return Result::WouldBlock;
            }
            return Result::Error;
        }
    }
    return Result::Success;
}

Socket::Result Socket::close()
{
    if(socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
        return Result::Success;
    }
    return Result::Error;
}