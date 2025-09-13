#include "StardustLib/ProtocolServer.hpp"
#include <cstring>
#include <iostream>
#include <vector>

namespace StardustLib
{
    ProtocolServer::ProtocolServer(uint16_t port)
    {
        mTCPServer = std::make_unique<TCPServer>(port);
        // tie callbacks
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
            mClientBuffers.clear();
        }
    }

    void ProtocolServer::onTcpPacket(const TCPServer::Packet& pkt)
    {
        std::lock_guard<std::mutex> lk(mClientBuffersMtx);
        auto &buf = mClientBuffers[pkt.clientId];
        buf.insert(buf.end(), pkt.data.begin(), pkt.data.end());
        parseClientBuffer(pkt.clientId, buf);
    }

    void ProtocolServer::onTcpDisconnect(uint64_t clientId)
    {
        {
            std::lock_guard<std::mutex> lk(mClientBuffersMtx);
            mClientBuffers.erase(clientId);
        }
        if (mDisconnectCb) mDisconnectCb(clientId);
    }

    void ProtocolServer::parseClientBuffer(uint64_t clientId, std::vector<uint8_t>& buf)
    {
        size_t offset = 0;
        while (true)
        {
            if (buf.size() - offset < 4) break; // need length

            uint32_t len = 0;
            std::memcpy(&len, buf.data() + offset, 4);
            uint32_t total_len = len;

            if (total_len == 0 || total_len > MAX_MESSAGE_SIZE)
            {
                // protocol error: drop connection
                std::cerr << "Protocol error: invalid frame length " << total_len << ". Disconnecting client " << clientId << ".\n";
                buf.clear();
                if (mDisconnectCb) mDisconnectCb(clientId);
                return;
            }
            if (buf.size() - offset < 4 + total_len) break; // wait for full frame

            // frame available
            const uint8_t* framePtr = buf.data() + offset + 4;
            size_t frameSize = total_len;

            if (frameSize < 4)
            { // must have at least opCode and version
                std::cerr << "Protocol error: frame size " << frameSize << " is too small. Disconnecting client " << clientId << ".\n";
                buf.clear();
                if (mDisconnectCb) mDisconnectCb(clientId);
                return;
            }
            uint16_t be_type = 0;
            uint16_t be_ver = 0;
            std::memcpy(&be_type, framePtr + 0, 2);
            std::memcpy(&be_ver, framePtr + 2, 2);
            uint16_t opCode = from_be(be_type);
            uint16_t version = from_be(be_ver);

            uint32_t key = (static_cast<uint32_t>(opCode) << 16) | version;
            GenericRecvHandler typedHandler;
            bool hasTypedHandler = false;
            {
                std::lock_guard<std::mutex> lk(mHandlersMtx);
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
                    std::cerr << "ProtocolServer Error: Typed handler registered for " << opCode << "v" << version << " but no deserializer was found. Disconnecting client " << clientId << ".\n";
                    buf.clear();
                    if (mDisconnectCb) mDisconnectCb(clientId);
                    return;
                }

                auto payload = std::make_shared<std::vector<uint8_t>>(framePtr + 4, framePtr + frameSize);
                BufferReader reader(payload);

                Result<std::any> deser_result = deserializer(reader);

                if (!deser_result.ok)
                {
                    std::cerr << "Deserializer error (code=" << static_cast<int>(deser_result.err) << "), disconnecting client " << clientId << "\n";
                    buf.clear();
                    if (mDisconnectCb) mDisconnectCb(clientId);
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
