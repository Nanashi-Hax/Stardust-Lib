#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <any>
#include "StardustLib/DataBuffer.hpp"

namespace StardustLib
{
    template<typename T>
    using Deserializer = std::function<void(DataBuffer& in, T& out)>;
    using GenericDeserializer = std::function<void(DataBuffer& in, std::any& out)>;
    using GenericDeserializerMap = std::unordered_map<MessageId, GenericDeserializer, MessageIdHash, MessageIdEqual>;

    class DeserializerRegistry
    {
    public:
        DeserializerRegistry() = default;

        template<typename T>
        void registerDeserializer(MessageId id, Deserializer<T> deserializer)
        {
            std::lock_guard<std::mutex> lock(mMtx);
            mMap[id] = deserializer;
        }

        GenericDeserializer getSerializer(MessageId id) const
        {
            std::lock_guard<std::mutex> lock(mMtx);
            auto it = mMap.find(id);
            if (it == mMap.end()) return nullptr;
            return it->second;
        }
    
    private:
        mutable std::mutex mMtx;
        GenericDeserializerMap mMap;
    };
}
