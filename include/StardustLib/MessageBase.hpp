#pragma once

#include <type_traits>
#include <optional>
#include "StardustLib/TCPServer.hpp"
#include "StardustLib/ISerializable.hpp"

namespace StardustLib
{
    class MessageBase : public ISerializable
    {
    private:
        uint32_t mClientId;
        std::shared_ptr<TCPServer> mServer;

    protected:
        uint32_t getClientId() { return mClientId; }
        std::shared_ptr<TCPServer> getServer() { return mServer; }

    public:
        MessageBase(uint32_t clientId, std::shared_ptr<TCPServer> server) : mClientId(clientId), mServer(server) {}

        virtual ~MessageBase() = default;

        virtual void process() {}

        void send()
        {
            if(!mClientId || !mServer) return;

            BufferWriter writer;
            serialize(writer);

            TCPServer::Packet packet;
            packet.clientId = mClientId;
            packet.data = writer.data();

            mServer->send(std::move(packet));
        }
    };

    template<typename T>
    concept Message = std::is_base_of_v<MessageBase, T>;
}