#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <any>
#include "StardustLib/SerializerRegistry.hpp"
#include "StardustLib/DeserializerRegistry.hpp"
#include "StardustLib/ProcessorRegistry.hpp"
#include "StardustLib/DataBuffer.hpp"
#include "StardustLib/TCPServer.hpp"

namespace StardustLib
{
    class ProtocolServer
    {
    private:
        void onTcpPacket(const TCPServer::Packet& packet);
        void onTcpDisconnect(uint64_t clientId);
        void dataDispatcher(uint64_t clientId, DataBuffer& payload);

        std::unique_ptr<TCPServer> mTCPServer;

        SerializerRegistry mSerializers;
        DeserializerRegistry mDeserializers;
        ProcessorRegistry mProcessors;

        std::mutex mClientBuffersMtx;
        std::unordered_map<uint64_t, DataBuffer> mClientBufferMap;

    public:
        static constexpr uint32_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB

        explicit ProtocolServer(uint16_t port);
        ~ProtocolServer();

        bool start();
        void stop();

        template<typename T>
        void registerHandler(MessageId id, Processor<T> processor)
        {
            mProcessors.registerProcessor<T>(id, std::move(processor));
        }

        template<typename T>
        void registerSerializer(MessageId id, Serializer<T> serializer)
        {
            mSerializers.registerSerializer<T>(id, std::move(serializer));
        }

        template<typename T>
        void registerDeserializer(MessageId id, Deserializer<T> deserializer)
        {
            mDeserializers.registerDeserializer<T>(id, std::move(deserializer));
        }

        bool sendRaw(uint64_t clientId, MessageId id, const DataBuffer& payload)
        {
            const uint32_t idSize = sizeof(MessageId::value);
            uint32_t payloadSize = payload.buffer().size();

            uint32_t totalSize = idSize + payloadSize;
            if (totalSize > MAX_MESSAGE_SIZE) return false;

            DataBuffer writer;
            writer.write<uint32_t>(idSize);
            writer.write<uint16_t>(id.value);
            writer.write<uint32_t>(payloadSize);
            writer.writeBlob(payload.buffer());

            TCPServer::Packet packet;
            packet.clientId = clientId;
            packet.data = std::move(payload.buffer());
            return mTCPServer->send(packet);
        }

        template<typename T>
        bool send(uint64_t clientId, MessageId id, const T& object)
        {
            Serializer serializer = mSerializers.getSerializer(id)
            if (!serializer) return false;

            BufferWriter payload;
            serializer(&object, payload);
            return sendMessageRaw(clientId, id, payload);
        }
    };
}
