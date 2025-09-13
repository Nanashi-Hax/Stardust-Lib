#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <cstring>
#include <type_traits>

namespace StardustLib
{
    class BufferWriter
    {
    private:
        std::vector<uint8_t> mBuffer;

    public:
        void writeBytes(const void* data, size_t n) noexcept
        {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
            mBuffer.insert(mBuffer.end(), ptr, ptr + n);
        }

        template<typename T>
        void write(T value) noexcept
        {
            static_assert(std::is_arithmetic_v<T>, "T must be a primitive arithmetic type");
            writeBytes(&value, sizeof(value));
        }

        void writeString(const std::string& s) noexcept
        {
            write<uint32_t>(static_cast<uint32_t>(s.size()));
            if (!s.empty()) writeBytes(s.data(), s.size());
        }

        void writeBlob(const std::vector<uint8_t>& v) noexcept
        {
            write<uint32_t>(static_cast<uint32_t>(v.size()));
            if (!v.empty()) writeBytes(v.data(), v.size());
        }
    };

    class BufferReader
    {
    private:
        std::vector<uint8_t> mBuffer;
        size_t mReadPos = 0;

        size_t remain(){ return mBuffer.size() - mReadPos; }
        uint8_t* currentPtr() { return mBuffer.data() + mReadPos; }

    public:
        BufferReader(const std::vector<uint8_t>& v)
        {
            mBuffer.insert(mBuffer.end(), v.begin(), v.end());
        }

        bool tryReadBytes(void* outData, size_t length) noexcept
        {
            if (remain() < length) return false;
            std::memcpy(outData, currentPtr(), length);
            mReadPos += length;
            return true;
        }

        template<typename T>
        bool tryRead(T& outData) noexcept
        {
            static_assert(std::is_arithmetic_v<T>, "T must be a primitive arithmetic type");
            if (mBuffer.size() < sizeof(T)) return false;
            tryReadBytes(&outData, sizeof(outData));
            return true;
        }

        bool tryReadString(std::string &outData) noexcept
        {
            uint32_t length;
            if (!tryRead<uint32_t>(length)) return false;
            if (remain() < length) return false;
            outData.resize(length);
            tryReadBytes(outData.data(), length);
            return true;
        }

        bool tryReadBlob(std::vector<uint8_t> &outData) noexcept
        {
            uint32_t length;
            if (!tryRead<uint32_t>(length)) return false;
            if (remain() < length) return false;
            outData.resize(length);
            tryReadBytes(outData.data(), length);
            return true;
        }
    };
}
