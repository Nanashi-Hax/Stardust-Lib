#pragma once

#include "Stardust/Socket.hpp"
#include <vector>
#include <deque>

class TCPServer
{
public:
    using DisconnectCallback = std::function<void(uint64_t clientId)>;

    struct Packet
    {
        uint64_t id;
        std::vector<uint8_t> data;
    };

private:
    struct Client
    {
        uint64_t id;
        Socket socket;

        std::vector<uint8_t> sendBuffer;
        std::mutex sendMutex;
    };

    uint16_t port;
    bool running = false;
    Socket listenSocket;
    std::vector<Client> clients;
    std::mutex clientsMtx;
    uint64_t clientCounter = 0;

    std::deque<Packet> recvQueue;
    std::mutex recvQueueMutex;

    DisconnectCallback disconnectCallback;
    
public:
    TCPServer(uint16_t port) : port(port) {}
    ~TCPServer() { stop(); }

    void setDisconnectCallback(DisconnectCallback cb) { disconnectCallback = std::move(cb); }

    bool start();
    void stop();
    bool isRunning() const { return running; }

    void runEventLoop(int timeoutMs = 100);

    bool queueSend(const Packet& packet);
    bool dequeueRecv(Packet& outPacket);
};