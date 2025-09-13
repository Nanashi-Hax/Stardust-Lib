#pragma once

#include <cstdint>
#include <unordered_map>

namespace StardustLib
{
    struct MessageId
    {
        uint16_t value;
        constexpr explicit MessageId(uint16_t v) : value(v) {}
        bool operator==(const MessageId& other) const noexcept { return value == other.value; }
    };

    struct MessageIdHash
    {
        size_t operator()(const MessageId& id) const noexcept
        {
            return std::hash<uint16_t>{}(id.value);
        }
    };

    struct MessageIdEqual
    {
        bool operator()(const MessageId& a, const MessageId& b) const noexcept
        {
            return a.value == b.value;
        }
    };
}