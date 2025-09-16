#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <any>
#include "StardustLib/Buffer.hpp"
#include "StardustLib/TCPServer.hpp"
#include "StardustLib/MessageBase.hpp"
#include "StardustLib/MessageFactory.hpp"

namespace StardustLib
{
    class MessageServer
    {
    private:
        MessageFactory mFactory;

        void onPacket(const TCPServer::Packet& packet)
        {
            BufferReader buffer(packet.data);
            dispatch(packet.clientId, buffer);
        }

        void dispatch(uint64_t clientId, BufferReader& reader)
        {
            uint32_t id = reader.read<uint32_t>();
            std::unique_ptr<MessageBase> message = mFactory.create(id, clientId, mTCPServer);
            message->deserialize(reader);
            message->process();
        }

        std::shared_ptr<TCPServer> mTCPServer;

    public:
        explicit MessageServer::MessageServer(uint16_t port)
        {
            mTCPServer = std::make_shared<TCPServer>(port);
            mTCPServer->setRecvCallback([this](const TCPServer::Packet& p)
            {
                this->onPacket(p);
            });
        }

        ~MessageServer()
        {
            stop();
        }
        
        template<typename T>
        void registerType(uint32_t id)
        {
            mFactory.registerType<T>(id);
        }

        bool start()
        {
            return mTCPServer->start();
        }

        void stop()
        {
            mTCPServer->stop();
        }
    };
}