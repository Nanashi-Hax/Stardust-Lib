#include "Stardust/TCPServer.hpp"

#include <algorithm>
#include <chrono>
#include <poll.h>
#include <nn/ac.h>

#include <whb/log.h>

namespace StardustLib
{
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
    
        acceptThread = std::jthread([this](std::stop_token token)
        {
            runAcceptLoop(token);
        });
        transferThread = std::jthread([this](std::stop_token token)
        {
            runTransferLoop(token);
        });
        processThread = std::jthread([this](std::stop_token token)
        {
            runProcessLoop(token);
        });
    
        if(initializeServerIPAddress())
        {
            if(serverIPAddressCallback) serverIPAddressCallback(serverIPAddress);
        }
    
        return true;
    }
    
    void TCPServer::stop()
    {
        acceptThread.request_stop();
        transferThread.request_stop();
        processThread.request_stop();
    
        if(acceptThread.joinable()) acceptThread.join();
        if(transferThread.joinable()) transferThread.join();
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
    
    void TCPServer::runAcceptLoop(std::stop_token token, int timeoutMs)
    {
        if (!listenSocket) return;
        int listenFd = listenSocket->getFd();
    
        while (!token.stop_requested())
        {
            pollfd pfd{};
            pfd.fd = listenFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
        
            int pret = poll(&pfd, 1, timeoutMs);
            if (pret < 0)
            {
                if (errno == EINTR) continue;
                WHBLogPrintf("[accept] poll error errno=%d", errno);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            if (pret == 0) {
                continue;
            }
        
            if (pfd.revents & POLLIN)
            {
                uint32_t outIPAddress = 0;
                std::unique_ptr<Socket> newSock;
                auto ares = listenSocket->accept(newSock, outIPAddress);
            
                WHBLogPrintf("[accept] result=%d newFd=%d ip=0x%08x", (int)ares, newSock ? newSock->getFd() : -1, outIPAddress);
            
                if (ares == Socket::Result::Success)
                {
                    if (!newSock || newSock->getFd() < 0)
                    {
                        WHBLogPrintf("[accept] accepted invalid socket, ignoring");
                    }
                    else
                    {
                        auto client = std::make_unique<Client>();
                        client->id = clientCounter++; // 可能なら atomic にする
                        client->socket = std::move(newSock);
                        {
                            std::lock_guard<std::mutex> lk(clientsMtx);
                            clients.push_back(std::move(client));
                            WHBLogPrintf("[accept] pushed clients.size=%d", (int)clients.size());
                        }
                        if (clientIPAddressCallback) clientIPAddressCallback(outIPAddress, clientCounter - 1);
                    }
                }
                else if (ares == Socket::Result::WouldBlock)
                {
                    // poll が偽陽性のときなど。短いスリープで busy-wait を防ぐ
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                else
                {
                    WHBLogPrintf("[accept] error ares=%d errno=%d", (int)ares, errno);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
            }
        }
    
        WHBLogPrintf("[accept] loop exit");
    }
    
    void TCPServer::runTransferLoop(std::stop_token token, int timeoutMs)
    {
        struct Snap { Client* client; int fd; bool wantWrite; };
    
        while (!token.stop_requested())
        {
            std::vector<Snap> snaps;
        
            // 1) clients のスナップショットを作る（ロック下）
            {
                std::lock_guard<std::mutex> lk(clientsMtx);
                snaps.reserve(clients.size());
                for (auto& up : clients)
                {
                    if (!up || !up->socket) continue;
                    int fd = up->socket->getFd();
                    if (fd < 0) continue;
                    bool wantWrite = false;
                    {
                        std::lock_guard<std::mutex> sendlk(up->sendMutex);
                        if (!up->sendQueue.empty()) wantWrite = true;
                    }
                    snaps.push_back({ up.get(), fd, wantWrite });
                    WHBLogPrintf("[snapshot] id=%llu fd=%d wantWrite=%d",
                                 (unsigned long long)up->id, fd, (int)wantWrite);
                }
            }
        
            if (snaps.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        
            // 2) pollfd 配列を作る（listen なし、clients のみ）
            std::vector<pollfd> pfds;
            pfds.reserve(snaps.size());
            for (auto &s : snaps)
            {
                pollfd pfd{};
                pfd.fd = s.fd;
                pfd.events = POLLIN | (s.wantWrite ? POLLOUT : 0);
                pfd.revents = 0;
                pfds.push_back(pfd);
            }
        
            int pret = poll(pfds.data(), pfds.size(), timeoutMs);
            if (pret < 0)
            {
                if (errno == EINTR) continue;
                if (errno == ENOTCONN || errno == EBADF) {
                    WHBLogPrintf("[transfer] poll warning errno=%d", errno);
                    continue;
                }
                WHBLogPrintf("[transfer] poll fatal errno=%d", errno);
                break;
            }
            if (pret == 0) continue;
        
            // 3) スナップショットに対応して安全に処理（pfds[i] <-> snaps[i]）
            for (size_t i = 0; i < pfds.size(); ++i)
            {
                auto &pfd = pfds[i];
                auto &snap = snaps[i];
                Client* client = snap.client;
                if (!client) continue; // safety
            
                WHBLogPrintf("[transfer] handling id=%llu fd=%d revents=0x%x",
                             (unsigned long long)client->id, pfd.fd, pfd.revents);
                
                // recv
                if (pfd.revents & POLLIN)
                {
                    std::vector<uint8_t> buf(0x1000);
                    ssize_t recvd = 0;
                    auto rres = client->socket->recv(buf.data(), buf.size(), recvd);
                    WHBLogPrintf("[transfer] recv id=%llu rres=%d recvd=%d", (unsigned long long)client->id, (int)rres, (int)recvd);
                
                    if (rres == Socket::Result::Success && recvd > 0)
                    {
                        Packet pkt;
                        pkt.clientId = client->id;
                        pkt.data.insert(pkt.data.end(), buf.begin(), buf.begin() + recvd);
                        {
                            std::lock_guard<std::mutex> qlk(queueMtx);
                            packetQueue.push(pkt);
                            queueCv.notify_one();
                        }
                    }
                    else if (rres == Socket::Result::Closed || rres == Socket::Result::Error)
                    {
                        WHBLogPrintf("[transfer] recv closed id=%llu", (unsigned long long)client->id);
                        if (disconnectCallback) disconnectCallback(client->id);
                        client->socket->close();
                    }
                }
            
                // send
                if (pfd.revents & POLLOUT)
                {
                    std::lock_guard<std::mutex> sendlk(client->sendMutex);
                    if (!client->sendQueue.empty())
                    {
                        ssize_t sent = 0;
                        auto &front = client->sendQueue.front();
                        auto sres = client->socket->send(front.data(), front.size(), sent);
                        WHBLogPrintf("[transfer] send id=%llu sres=%d sent=%d front=%d",
                                     (unsigned long long)client->id, (int)sres, (int)sent, (int)front.size());
                        
                        if (sres == Socket::Result::Success || sres == Socket::Result::WouldBlock)
                        {
                            if ((size_t)sent == front.size()) client->sendQueue.pop_front();
                            else if (sent > 0) front.erase(front.begin(), front.begin() + sent);
                        }
                        else
                        {
                            WHBLogPrintf("[transfer] send closed id=%llu", (unsigned long long)client->id);
                            if (disconnectCallback) disconnectCallback(client->id);
                            client->socket->close();
                        }
                    }
                }
            } // for pfds/snaps
        
            // 4) cleanup invalid clients（clients ベクタはロックして扱う）
            {
                std::lock_guard<std::mutex> lk(clientsMtx);
                for (auto it = clients.begin(); it != clients.end(); )
                {
                    if (!(*it) || !(*it)->socket || (*it)->socket->getFd() < 0)
                    {
                        WHBLogPrintf("[transfer] cleanup erase id=%llu", (*it) ? (unsigned long long)(*it)->id : (unsigned long long)-1);
                        if (*it && (*it)->socket) (*it)->socket->close();
                        it = clients.erase(it);
                    }
                    else ++it;
                }
            }
        } // while
    }
    
    void TCPServer::runProcessLoop(std::stop_token token)
    {
        while(!token.stop_requested())
        {
            Packet packet;
            {
                std::unique_lock<std::mutex> lock(queueMtx);
                queueCv.wait(lock, [this, &token]
                {
                    return !packetQueue.empty() || token.stop_requested();
                });
            
                if(token.stop_requested()) break;
                if(packetQueue.empty()) continue;
            
                packet = packetQueue.front();
                packetQueue.pop();
            }
        
            if(recvCallback) recvCallback(packet);
        }
    }
}