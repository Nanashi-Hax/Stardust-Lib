#pragma once

#include "StardustLib/Buffer.hpp"

namespace StardustLib
{
    class ISerializable
    {
    public:
        virtual ~ISerializable() = default;

        virtual void serialize(BufferWriter& writer) const = 0;

        virtual void deserialize(BufferReader& reader) = 0;
    };
}