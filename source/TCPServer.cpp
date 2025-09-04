#include "Stardust/TCPServer.hpp"
#include <poll.h>
#include <algorithm>

bool TCPServer::start()
{
    if(listenSocket.create(true, true) != Socket::Result::Success) return false;
    if(listenSocket.bind(port) != Socket::Result::Success) return false;
    if(listenSocket.listen() != Socket::Result::Success) return false;
    running = true;
    return true;
}

void TCPServer::stop()
{
    running = false;
    for(auto& client : clients) client.socket.close();
    clients.clear();
    listenSocket.close();
}

bool TCPServer::queueSend(const Packet& packet)
{
    Client* client = nullptr;

    {
        std::lock_guard<std::mutex> lock(clientsMtx);

        for(auto& c : clients)
        {
            if(c.id == packet.id)
            {
                client = &c;
                break;
            }
        }
    }

    if(!client) return false;

    {
        std::lock_guard<std::mutex> sendLock(client->sendMutex);

        client->sendBuffer.insert(client->sendBuffer.end(), packet.data.begin(), packet.data.end());
        return true;
    }
}

bool TCPServer::dequeueRecv(Packet& outPacket)
{
    std::lock_guard<std::mutex> lock(recvQueueMutex);
    if (recvQueue.empty()) return false;

    outPacket = std::move(recvQueue.front());
    recvQueue.pop_front();
    return true;
}

void TCPServer::runEventLoop(int timeoutMs)
{
    while(running)
    {
        std::vector<pollfd> pfds;

        // listen sockets
        pollfd listenPfd;
        listenPfd.fd = listenSocket.getFd();
        listenPfd.events = POLLIN;
        listenPfd.revents = 0;
        pfds.push_back(listenPfd);

        // client sockets
        {
            std::lock_guard<std::mutex> clientLock(clientsMtx);
            for(auto& client : clients)
            {
                pollfd pfd;
                pfd.fd = client.socket.getFd();
                pfd.events = POLLIN;

                {
                    std::lock_guard<std::mutex> sendLock(client.sendMutex);
                    if(!client.sendBuffer.empty()) pfd.events |= POLLOUT;
                }

                pfd.revents = 0;
                pfds.push_back(pfd);
            }
        }

        int ret = poll(pfds.data(), pfds.size(), timeoutMs);
        if(ret < 0) break;
        if(ret == 0) continue;
        
        // accept new clients
        if(pfds[0].revents & POLLIN)
        {
            Socket newClient;
            auto res = listenSocket.accept(newClient);
            if(res == Socket::Result::Success)
            {
                Client client;
                client.id = clientCounter++;
                client.socket = std::move(newClient);
                std::lock_guard<std::mutex> clientLock(clientsMtx);
                clients.push_back(std::move(client));
            }
        }

        // handle clients
        {
            std::lock_guard<std::mutex> clientLock(clientsMtx);

            for(size_t i = 0; i < clients.size(); i++)
            {
                auto& client = clients[i];
                pollfd& pfd = pfds[i + 1];

                // recv
                if(pfd.revents & POLLIN)
                {
                    uint8_t buffer[0x1000];
                    int bytes = 0;
                    auto res = client.socket.recv(buffer, sizeof(buffer), bytes);

                    if(res == Socket::Result::Success && bytes > 0)
                    {
                        Packet pkt;
                        pkt.id = client.id;
                        pkt.data.assign(buffer, buffer + bytes);

                        std::lock_guard<std::mutex> recvLock(recvQueueMutex);
                        recvQueue.push_back(std::move(pkt));
                    }
                    else if(res == Socket::Result::Closed || res == Socket::Result::Error)
                    {
                        if(disconnectCallback) disconnectCallback(client.id);
                        client.socket.close();
                    }
                }

                // send
                if(pfd.revents & POLLOUT)
                {
                    std::lock_guard<std::mutex> sendlock(client.sendMutex);

                    if(!client.sendBuffer.empty())
                    {
                        ssize_t sentBytes = 0;
                        auto res = client.socket.send(client.sendBuffer.data(), client.sendBuffer.size(), sentBytes);

                        if(res == Socket::Result::Success || res == Socket::Result::WouldBlock)
                        {
                            if(sentBytes > 0)
                            {
                                client.sendBuffer.erase(client.sendBuffer.begin(), client.sendBuffer.begin() + sentBytes);
                            }
                        }
                        else if(res == Socket::Result::Closed || res == Socket::Result::Error)
                        {
                            if(disconnectCallback) disconnectCallback(client.id);
                            client.socket.close();
                        }
                    }
                }
            }

            clients.erase(std::remove_if(clients.begin(), clients.end(), [](const Client& client){ return client.socket.getFd() < 0; }), clients.end());
        }
    }
}