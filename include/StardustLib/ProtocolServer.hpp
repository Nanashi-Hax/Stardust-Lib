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
#include "StardustLib/Buffer.hpp"
#include "StardustLib/TCPServer.hpp"

namespace StardustLib
{
    class ProtocolServer
    {
    private:
        // internal helpers
        void onTcpPacket(const TCPServer::Packet& pkt);
        void onTcpDisconnect(uint64_t clientId);
        void parseClientBuffer(uint64_t clientId, std::vector<uint8_t>& buffer);

        std::unique_ptr<TCPServer> mTCPServer;

        // Registries and Callbacks
        SerializerRegistry mSerializers;
        DeserializerRegistry mDeserializers;
        ProcessorRegistry mProcessors;

        std::mutex mClientBuffersMtx;
        std::unordered_map<uint64_t, std::vector<uint8_t>> mClientBuffers;

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

        bool sendMessageRaw(uint64_t clientId, MessageId id, const BufferWriter& payload)
        {
            // Frame: [u32 total_len_be] [u16 opCode_be] [u16 version_be] [payload]
            // total_len = 2 (type) + 2 (ver) + N (payload)
            uint32_t payloadSize = static_cast<uint32_t>(payload.buf.size());
            uint32_t totalLen = 2 + 2 + payloadSize;
            if (totalLen > MAX_MESSAGE_SIZE) return false;

            BufferWriter w;
            w.reserve(4 + totalLen);
            w.write<uint32_t>(totalLen);
            w.write<uint16_t>(opCode);
            w.write<uint16_t>(version);
            if (payloadSize) w.writeBytes(payload.buf.data(), payloadSize);

            TCPServer::Packet pkt;
            pkt.clientId = clientId;
            pkt.data = std::move(w.buf);
            return mTCPServer->send(pkt);
        }

        template<typename T>
        bool send(uint64_t clientId, MessageId id, const T& object)
        {
            std::function<void(const void* obj, BufferWriter& out)> ser;
            if (!mSerializers.getSerializer(opCode, version, ser)) return false;
            BufferWriter payload;
            ser(&object, payload);
            return sendMessageRaw(clientId, opCode, version, payload);
        }

        template<typename T>
        bool send(uint64_t clientId, const T& obj)
        {
            uint16_t opCode = 0, version = 0;
            if (!mSerializers.getKeyForType<T>(opCode, version)) return false;
            return send<T>(clientId, opCode, version, obj);
        }
    };
} // namespace StardustLib
