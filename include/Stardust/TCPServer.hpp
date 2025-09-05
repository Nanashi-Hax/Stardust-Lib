#pragma once

#include "Stardust/Socket.hpp"
#include <vector>
#include <queue>
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
    using ServerIPAddressCallback = std::function<void(uint32_t ipAddress)>;
    using ClientIPAddressCallback = std::function<void(uint32_t ipAddress, uint64_t id)>;

private:
    struct Client
    {
        uint64_t id;
        std::unique_ptr<Socket> socket;

        std::deque<std::vector<uint8_t>> sendQueue;
        std::mutex sendMutex;
    };

    uint32_t serverIPAddress;
    uint16_t port;

    std::unique_ptr<Socket> listenSocket;
    std::vector<std::unique_ptr<Client>> clients;
    std::mutex clientsMtx;
    uint64_t clientCounter = 0;

    RecvCallback recvCallback;
    DisconnectCallback disconnectCallback;
    ServerIPAddressCallback serverIPAddressCallback;
    ClientIPAddressCallback clientIPAddressCallback;

    std::queue<Packet> packetQueue;
    std::mutex queueMtx;
    std::condition_variable queueCv;
    std::jthread processThread;
    std::jthread networkThread;

    void runNetworkLoop(std::stop_token st, int timeoutMs = 100);
    void runProcessLoop(std::stop_token st);

    bool initializeServerIPAddress();
    void finalizeServerIPAddress();
    
public:
    TCPServer(uint16_t port) : port(port) {}
    ~TCPServer() { stop(); }

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
    TCPServer(TCPServer&&) = delete;
    TCPServer& operator=(TCPServer&&) = delete;

    bool start();
    void stop();

    bool send(const Packet& packet);

    void setRecvCallback(RecvCallback cb) { recvCallback = cb; }
    void setDisconnectCallback(DisconnectCallback cb) { disconnectCallback = std::move(cb); }
    void setServerIPAddressCallback(ServerIPAddressCallback cb) { serverIPAddressCallback = std::move(cb); }
    void setClientIPAddressCallback(ClientIPAddressCallback cb) { clientIPAddressCallback = std::move(cb); }
};