#pragma once

#include <cstdint>
#include <vector>

class ISerializer
{
public:
    virtual ~ISerializer() = default;

    // デシリアライズ: バイナリデータ → オブジェクト
    virtual void* deserialize(const std::vector<uint8_t>& data) const = 0;

    // シリアライズ: オブジェクト → バイナリデータ
    virtual std::vector<uint8_t> serialize(const void* obj) const = 0;
};