#include <iostream>
#include <thread>
#include <chrono>
#include "StardustLib/MessageBase.hpp"
#include "StardustLib/MessageServer.hpp"

class Ping : public StardustLib::MessageBase
{
private:
    uint32_t value;

public:
    void serialize(StardustLib::BufferWriter& writer) const override
    {
        writer.write(value);
    }

    void deserialize(StardustLib::BufferReader& reader) override
    {
        value = reader.read<uint32_t>();
    }

    void process() override
    {
        Pong pong(getClientId(), getServer());
        pong.send();
    }
};

class Pong : public StardustLib::MessageBase
{
private:
    uint32_t value;

public:
    void serialize(StardustLib::BufferWriter& writer) const override
    {
        writer.write(value);
    }

    void deserialize(StardustLib::BufferReader& reader) override
    {
        value = reader.read<uint32_t>();
    }
};

int main()
{
    uint16_t port = 12345;
    StardustLib::MessageServer server(port);

    server.registerType<Ping>(1);

    server.start();

    std::cin.get();

    server.stop();

    return 0;
}
