#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <any>
#include "StardustLib/Buffer.hpp"
#include "StardustLib/Message.hpp"

namespace StardustLib
{
    template<typename T>
    using Serializer = std::function<void(T in, BufferWriter& out)>;
    using GenericSerializer = std::function<void(std::any in, BufferWriter& out)>;
    using GenericSerializerMap = std::unordered_map<MessageId, GenericSerializer, MessageIdHash, MessageIdEqual>;

    class SerializerRegistry
    {
    public:
        SerializerRegistry() = default;

        template<typename T>
        void registerSerializer(MessageId id, Serializer<T> serializer)
        {
            std::lock_guard<std::mutex> lock(mMtx);
            mMap[id] = serializer;
        }

        GenericSerializer getSerializer(MessageId id) const
        {
            std::lock_guard<std::mutex> lock(mMtx);
            auto it = mMap.find(id);
            if (it == mMap.end()) return nullptr;
            return it->second;
        }

    private:
        mutable std::mutex mMtx;
        GenericSerializerMap mMap;
    };
}
