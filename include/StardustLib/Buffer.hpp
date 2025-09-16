#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <bit>

namespace StardustLib
{
    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    T swapEndianIfLE(T value)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            return value;
        }
        else
        {
            T result{};
            auto src = reinterpret_cast<const uint8_t*>(&value);
            auto dst = reinterpret_cast<uint8_t*>(&result);
            for (size_t i = 0; i < sizeof(T); i++)
            {
                dst[i] = src[sizeof(T) - 1 - i];
            }
            return result;
        }
    }

    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    T toBigEndian(T value)
    {
        return swapEndianIfLE(value);
    }

    template<typename T> requires(std::integral<T> || std::floating_point<T>)
    T fromBigEndian(T value)
    {
        return swapEndianIfLE(value);
    }

    class BufferWriter
    {
    private:
        std::vector<uint8_t> mBuffer;
    
    public:
        BufferWriter() = default;

        const std::vector<uint8_t>& data() const noexcept { return mBuffer; }

        template<typename T> requires(std::integral<T> || std::floating_point<T>)
        void write(T value)
        {
            T valueBE = toBigEndian(value);
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&valueBE);
            mBuffer.insert(mBuffer.end(), ptr, ptr + sizeof(T));
        }
    };

    class BufferReader
    {
    private:
        std::vector<uint8_t> mBuffer;
        size_t mPos = 0;
    
    public:
        BufferReader(std::vector<uint8_t> v) : mBuffer(std::move(v)) {}

        bool eof() const noexcept { return mPos >= mBuffer.size(); }

        template<typename T> requires(std::integral<T> || std::floating_point<T>)
        T read()
        {
            T value{};
            
            if(mPos + sizeof(T) > mBuffer.size()) return value;

            std::memcpy(&value, mBuffer.data() + mPos, sizeof(T));
            mPos += sizeof(T);

            return fromBigEndian(value);
        }
    };
}
