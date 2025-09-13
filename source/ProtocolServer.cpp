#include "StardustLib/ProtocolServer.hpp"
#include <cstring>
#include <iostream>
#include <vector>

namespace StardustLib
{
    ProtocolServer::ProtocolServer(uint16_t port)
    {
        mTCPServer = std::make_unique<TCPServer>(port);
        mTCPServer->setRecvCallback([this](const TCPServer::Packet& p)
        {
            this->onTcpPacket(p);
        });
        mTCPServer->setDisconnectCallback([this](uint64_t id)
        {
            this->onTcpDisconnect(id);
        });
    }

    ProtocolServer::~ProtocolServer()
    {
        stop();
    }

    bool ProtocolServer::start()
    {
        return mTCPServer->start();
    }

    void ProtocolServer::stop()
    {
        mTCPServer->stop();
        {
            std::lock_guard<std::mutex> lk(mClientBuffersMtx);
            mClientBufferMap.clear();
        }
    }

    void ProtocolServer::onTcpPacket(const TCPServer::Packet& packet)
    {
        std::lock_guard<std::mutex> lock(mClientBuffersMtx);
        DataBuffer& buffer = mClientBufferMap[packet.clientId];
        buffer.insert(buffer.end(), packet.data.begin(), packet.data.end());
        dataDispatcher(packet.clientId, buffer);
    }

    void ProtocolServer::dataDispatcher(uint64_t clientId, DataBuffer& data)
    {
        size_t offset = 0;
        while (true)
        {
            uint32_t length = 0;
            if (data.size() - offset < sizeof(length)) break;

            data.read<uint32_t>(length);
            if (length == 0 || length > MAX_MESSAGE_SIZE) return;

            if (data.size() - offset < sizeof(length) + length) break;

            GenericProcessor processor;
            bool hasProcessor = false;
            {
                std::lock_guard<std::mutex> lk(mProcessorMtx);
                auto it = mHandlers.find(key);
                if (it != mHandlers.end())
                {
                    hasTypedHandler = true;
                    typedHandler = it->second;
                }
            }

            if (hasTypedHandler)
            {
                // New path: use deserializer and typed handler
                GenericDeserializer deserializer;
                if (!mDeserializers.getDeserializer(opCode, version, deserializer))
                {
                    buf.clear();
                    return;
                }

                auto payload = std::make_shared<std::vector<uint8_t>>(framePtr + 4, framePtr + frameSize);
                BufferReader reader(payload);

                deserializer(reader);

                if (!deser_result.ok)
                {
                    buf.clear();
                    return;
                }

                ErrorCode ec = typedHandler(clientId, deser_result.value);
                if (ec != ErrorCode::Ok)
                {
                    std::cerr << "Typed handler error (code=" << static_cast<int>(ec) << "), disconnecting client " << clientId << "\n";
                    buf.clear();
                    if (mDisconnectCb) mDisconnectCb(clientId);
                    return;
                }

            }
            else
            {
                std::cerr << "Unknown message type " << opCode << " v" << version << " from client " << clientId << "\n";
            }

            offset += 4 + total_len;
        }

        // remove consumed bytes
        if (offset > 0)
        {
            buf.erase(buf.begin(), buf.begin() + offset);
        }
    }
} // namespace StardustLib
