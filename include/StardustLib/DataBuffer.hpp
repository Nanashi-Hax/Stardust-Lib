#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <string>
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace StardustLib
{
    class DataBuffer
    {
    private:
        std::list<uint8_t> mBuffer;

    public:
        void writeRaw(const void* data, const size_t length) noexcept
        {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
            mBuffer.insert(mBuffer.end(), ptr, ptr + length);
        }

        template<typename T>
        void write(T value) noexcept
        {
            static_assert(std::is_arithmetic_v<T>, "T must be a primitive arithmetic type");
            writeRaw(&value, sizeof(value));
        }

        // length[4] + char[n]
        void writeString(const std::string& s) noexcept
        {
            write<uint32_t>(static_cast<uint32_t>(s.size()));
            if (!s.empty()) writeRaw(s.data(), s.size());
        }

        // length[4] + data[n]
        void writeBlob(const std::vector<uint8_t>& v) noexcept
        {
            write<uint32_t>(static_cast<uint32_t>(v.size()));
            if (!v.empty()) writeRaw(v.data(), v.size());
        }

        bool readRaw(void* outData, const size_t length) noexcept
        {
            if (mBuffer.size() < length) return false;
        
            uint8_t* ptr = reinterpret_cast<uint8_t*>(outData);
            auto it = mBuffer.begin();
        
            std::copy_n(it, length, ptr);
            std::advance(it, length);
            mBuffer.erase(mBuffer.begin(), it);
        
            return true;
        }

        template<typename T>
        bool read(T& outValue) noexcept
        {
            static_assert(std::is_arithmetic_v<T>, "T must be a primitive arithmetic type");
            size_t typeSize = sizeof(T);
            if (mBuffer.size() < typeSize) return T();
            return readRaw(&outValue, typeSize);
        }

        bool readString(std::string &outData) noexcept
        {
            uint32_t length;
            if (!read<uint32_t>(length)) return false;
            if (mBuffer.size() < length) return false;
            outData.resize(length);
            readRaw(outData.data(), length);
            return true;
        }

        bool readBlob(std::vector<uint8_t> &outData) noexcept
        {
            uint32_t length;
            if (!read<uint32_t>(length)) return false;
            if (mBuffer.size() < length) return false;
            outData.resize(length);
            readRaw(outData.data(), length);
            return true;
        }

        std::list<uint8_t>::iterator begin() noexcept { return mBuffer.begin(); }
        std::list<uint8_t>::iterator end() noexcept { return mBuffer.end(); }

        void insert(std::list<uint8_t>::const_iterator pos, std::vector<uint8_t>::const_iterator first, std::vector<uint8_t>::const_iterator last) noexcept
        {
            mBuffer.insert(pos, first, last);
        }

        size_t size() const noexcept { return mBuffer.size(); }
    };
}
