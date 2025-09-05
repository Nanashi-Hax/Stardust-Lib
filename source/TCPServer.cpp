#include "Stardust/TCPServer.hpp"
#include <poll.h>
#include <algorithm>
#include <nn/ac.h>

bool TCPServer::initializeServerIPAddress()
{
    if (nn::ac::Initialize().IsFailure()) return false;
    if (nn::ac::Connect().IsFailure()) return false;
    if (nn::ac::GetAssignedAddress(&serverIPAddress).IsFailure()) return false;
    return true;
}

void TCPServer::finalizeServerIPAddress()
{
    nn::ac::Finalize();
}

bool TCPServer::start()
{
    listenSocket = std::make_unique<Socket>();
    if(listenSocket->create(true, true) != Socket::Result::Success) return false;
    if(listenSocket->bind(port) != Socket::Result::Success) return false;
    if(listenSocket->listen() != Socket::Result::Success) return false;

    networkThread = std::jthread([this](std::stop_token st)
    {
        runNetworkLoop(st);
    });
    processThread = std::jthread([this](std::stop_token st)
    {
        runProcessLoop(st);
    });

    if(initializeServerIPAddress())
    {
        if(serverIPAddressCallback) serverIPAddressCallback(serverIPAddress);
    }

    return true;
}

void TCPServer::stop()
{
    networkThread.request_stop();
    processThread.request_stop();

    if(networkThread.joinable()) networkThread.join();
    if(processThread.joinable()) processThread.join();

    if(listenSocket) listenSocket->close();

    std::lock_guard<std::mutex> lock(clientsMtx);
    for(auto& client : clients)
    {
        if(client && client->socket)
        {
            client->socket->close();
        }
    }
    clients.clear();

    finalizeServerIPAddress();
}

bool TCPServer::send(const Packet& packet)
{
    Client* client = nullptr;

    {
        std::lock_guard<std::mutex> lock(clientsMtx);
        for(auto& c : clients)
        {
            if(c->id == packet.clientId)
            {
                client = c.get();
                break;
            }
        }
    }

    if(!client) return false;

    {
        std::lock_guard<std::mutex> sendLock(client->sendMutex);

        client->sendQueue.push_back(packet.data);
    }
    return true;
}

void TCPServer::runNetworkLoop(std::stop_token st, int timeoutMs)
{
    while(!st.stop_requested())
    {
        std::vector<pollfd> pfds;

        // listen sockets
        pollfd listenPfd;
        listenPfd.fd = listenSocket->getFd();
        listenPfd.events = POLLIN;
        listenPfd.revents = 0;
        pfds.push_back(listenPfd);

        // client sockets
        {
            std::lock_guard<std::mutex> clientLock(clientsMtx);
            for(auto& client : clients)
            {
                pollfd pfd;
                pfd.fd = client->socket->getFd();
                pfd.events = POLLIN;

                {
                    std::lock_guard<std::mutex> sendLock(client->sendMutex);
                    if(!client->sendQueue.empty()) pfd.events |= POLLOUT;
                }

                pfd.revents = 0;
                pfds.push_back(pfd);
            }
        }

        int ret = poll(pfds.data(), pfds.size(), timeoutMs);
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        if(ret == 0) continue;
        
        // accept new clients
        if(pfds[0].revents & POLLIN)
        {
            uint32_t outIPAddress = 0;
            std::unique_ptr<Socket> newClient;
            auto res = listenSocket->accept(newClient, outIPAddress);
            if(res == Socket::Result::Success)
            {
                std::unique_ptr<Client> client = std::make_unique<Client>();
                client->id = clientCounter++;
                client->socket = std::move(newClient);
                std::lock_guard<std::mutex> clientLock(clientsMtx);
                clients.push_back(std::move(client));
                if(clientIPAddressCallback) clientIPAddressCallback(outIPAddress, client->id);
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
                    auto res = client->socket->recv(buffer, sizeof(buffer), bytes);

                    if(res == Socket::Result::Success && bytes > 0)
                    {
                        Packet packet;
                        packet.clientId = client->id;
                        packet.data.assign(buffer, buffer + bytes);
                        
                        {
                            std::lock_guard<std::mutex> queueLock(queueMtx);
                            packetQueue.push(packet);
                            queueCv.notify_one();
                        }
                    }
                    else if(res == Socket::Result::Closed || res == Socket::Result::Error)
                    {
                        if(disconnectCallback) disconnectCallback(client->id);
                        client->socket->close();
                    }
                }

                // send
                if(pfd.revents & POLLOUT)
                {
                    std::lock_guard<std::mutex> sendlock(client->sendMutex);

                    if(!client->sendQueue.empty())
                    {
                        ssize_t sentBytes = 0;
                        auto& front = client->sendQueue.front();
                        auto res = client->socket->send(front.data(), front.size(), sentBytes);

                        if(res == Socket::Result::Success || res == Socket::Result::WouldBlock)
                        {
                            if(static_cast<size_t>(sentBytes) == front.size())
                            {
                                client->sendQueue.pop_front();
                            }
                            else if(sentBytes > 0)
                            {
                                front.erase(front.begin(), front.begin() + sentBytes);
                            }
                        }
                        else if(res == Socket::Result::Closed || res == Socket::Result::Error)
                        {
                            if(disconnectCallback) disconnectCallback(client->id);
                            client->socket->close();
                        }
                    }
                }
            }

            for(auto it = clients.begin(); it != clients.end(); )
            {
                if((*it)->socket->getFd() < 0)
                {
                    (*it)->socket->close();
                    it = clients.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }
    }
}

void TCPServer::runProcessLoop(std::stop_token st)
{
    while(!st.stop_requested())
    {
        Packet packet;
        {
            std::unique_lock<std::mutex> lock(queueMtx);
            queueCv.wait(lock, [this, &st]
            {
                return !packetQueue.empty() || st.stop_requested();
            });

            if(st.stop_requested()) break;
            if(packetQueue.empty()) continue;

            packet = packetQueue.front();
            packetQueue.pop();
        }

        if(recvCallback) recvCallback(packet);
    }
}