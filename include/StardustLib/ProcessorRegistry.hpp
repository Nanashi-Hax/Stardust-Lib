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
    using Processor = std::function<void(uint64_t clientId, const T& data)>;
    using GenericProcessor = std::function<void(uint64_t clientId, const std::any& data)>;
    using GenericProcessorMap = std::unordered_map<MessageId, GenericProcessor, MessageIdHash, MessageIdEqual>;

    class ProcessorRegistry
    {
    public:
        ProcessorRegistry() = default;

        template<typename T>
        void registerProcessor(MessageId id, Processor<T> processor)
        {
            std::lock_guard<std::mutex> lock(mMtx);
            mMap[id] = processor;
        }
    
        GenericProcessor getSerializer(MessageId id) const
        {
            std::lock_guard<std::mutex> lock(mMtx);
            auto it = mMap.find(id);
            if (it == mMap.end()) return nullptr;
            return it->second;
        }

    private:
        mutable std::mutex mMtx;
        GenericProcessorMap mMap;
    };
}
