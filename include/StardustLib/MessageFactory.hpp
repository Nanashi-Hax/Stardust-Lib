#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include "StardustLib/MessageBase.hpp"

namespace StardustLib
{
    class MessageFactory
    {
    public:
        template<Message T>
        void registerType(uint32_t id)
        {
            creators[id] = [](uint32_t clientId, std::shared_ptr<TCPServer> server){ return std::make_unique<T>(clientId, server); };
        }

        std::unique_ptr<MessageBase> create(uint32_t id, uint32_t clientId, std::shared_ptr<TCPServer> server) const
        {
            auto it = creators.find(id);
            if (it != creators.end())
            {
                return (it->second)(clientId, server);
            }
            return nullptr;
        }

    private:
        using Creator = std::function<std::unique_ptr<MessageBase>(uint32_t clientId, std::shared_ptr<TCPServer> server)>;
        std::unordered_map<uint32_t, Creator> creators;
    };
}