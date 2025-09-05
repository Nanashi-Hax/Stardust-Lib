#include "Stardust/Socket.hpp"

#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

Socket::Result Socket::create(bool nonBlocking, bool noDelay)
{
    socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socketFd < 0)
    {
        close();
        return Result::Error;
    }

    int ret;

    int opt = 1;
    ret = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret < 0)
    {
        close();
        return Result::Error;
    }

    if(noDelay)
    {
        ret = setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        if(ret < 0)
        {
            close();
            return Result::Error;
        }
    }

    if(nonBlocking)
    {
        int flags = fcntl(socketFd, F_GETFL, 0);
        if(flags < 0)
        {
            close();
            return Result::Error;
        }
        ret = fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
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

    int res;
    {
        std::lock_guard<std::mutex> bindLock(mutex);
        res = ::bind(socketFd, (struct sockaddr*)&addr, sizeof(addr));
    }

    if(res < 0)
    {
        close();
        return Result::Error;
    }

    return Result::Success;
}

Socket::Result Socket::listen(int backlog)
{
    int res;
    {
        std::lock_guard<std::mutex> listenLock(mutex);
        res = ::listen(socketFd, backlog);
    }

    if(res < 0)
    {
        close();
        return Result::Error;
    }

    return Result::Success;
}

Socket::Result Socket::accept(std::unique_ptr<Socket>& outClient)
{
    int clientFd;
    {
        std::lock_guard<std::mutex> acceptLock(mutex);
        clientFd = ::accept(socketFd, nullptr, nullptr);
    }

    if(clientFd >= 0)
    {
        outClient = std::make_unique<Socket>();
        outClient->socketFd = clientFd;
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
    if(socketFd < 0) return Result::Error;

    ssize_t sent;
    {
        std::lock_guard<std::mutex> sendLock(mutex);
        sent = ::send(socketFd, data, size, 0);
    }

    if(sent > 0)
    {
        outBytes = sent;
        return Result::Success;
    }
    else if(sent == 0)
    {
        outBytes = 0;
        return Result::Closed;
    }
    else
    {
        outBytes = 0;
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return Result::WouldBlock;
        }
        return Result::Error;
    }
}

Socket::Result Socket::recv(void* buffer, ssize_t size, ssize_t& outBytes)
{
    if(socketFd < 0) return Result::Error;

    ssize_t recvd;
    {
        std::lock_guard<std::mutex> recvLock(mutex);
        recvd = ::recv(socketFd, buffer, size, 0);
    }

    if(recvd > 0)
    {
        outBytes = recvd;
        return Result::Success;
    }
    else if(recvd == 0)
    {
        outBytes = 0;
        return Result::Closed;
    }
    else
    {
        outBytes = 0;
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return Result::WouldBlock;
        }
        return Result::Error;
    }
}

Socket::Result Socket::close()
{
    if(socketFd >= 0)
    {
        std::lock_guard<std::mutex> closeLock(mutex);
        ::close(socketFd);
        socketFd = -1;
    }
    return Result::Success;
}

int Socket::getFd() const
{
    return socketFd;
}