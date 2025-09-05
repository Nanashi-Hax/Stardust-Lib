#pragma once

#include "Stardust/Socket.hpp"
#include <vector>
#include <deque>
#include <thread>

class TCPServer
{
public:
    struct Packet
    {
        uint64_t clientId;
        std::vector<uint8_t> data;
    };

    using RecvCallback = std::function<void(const Packet& data)>;
    using DisconnectCallback = std::function<void(uint64_t clientId)>;

private:
    struct Client
    {
        uint64_t id;
        std::unique_ptr<Socket> socket;

        std::deque<std::vector<uint8_t>> sendQueue;
        std::mutex sendMutex;
    };

    uint16_t port;

    std::unique_ptr<Socket> listenSocket;
    std::vector<std::unique_ptr<Client>> clients;
    std::mutex clientsMtx;
    uint64_t clientCounter = 0;

    RecvCallback recvCallback;
    DisconnectCallback disconnectCallback;

    std::jthread eventLoopThread;
    void runEventLoop(std::stop_token st, int timeoutMs = 100);
    
public:
    TCPServer(uint16_t port) : port(port) {}
    ~TCPServer() { stop(); }

    bool start();
    void stop();

    bool send(const Packet& packet);

    void setRecvCallback(RecvCallback cb) { recvCallback = cb; }
    void setDisconnectCallback(DisconnectCallback cb) { disconnectCallback = std::move(cb); }
};