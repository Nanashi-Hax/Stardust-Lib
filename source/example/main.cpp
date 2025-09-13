// StardustLib Usage Example (New API)
//
// This is a sample implementation of a Ping-Pong server
// to demonstrate the usage of the new typed-handler API.

#include <iostream>
#include <thread>
#include <chrono>
#include "StardustLib/ProtocolServer.hpp"
#include "StardustLib/Buffer.hpp"

// 1. Define message structures
struct PingMessage
{
    uint32_t value;
};

struct PongMessage
{
    uint32_t value;
};

// 2. Define message types and versions
constexpr uint16_t MSG_TYPE_PING = 1;
constexpr uint16_t MSG_TYPE_PONG = 2;

int main()
{
    // 3. Create a ProtocolServer instance
    uint16_t port = 12345;
    StardustLib::ProtocolServer server(port);

    // 4. Register a DESERIALIZER for incoming Ping messages
    server.registerDeserializer<PingMessage>(MSG_TYPE_PING,
        [](StardustLib::BufferReader& reader) -> StardustLib::Result<PingMessage>
        {
            PingMessage msg;
            if (!reader.tryRead<uint32_t>(msg.value))
            {
                return StardustLib::Result<PingMessage>::failure(StardustLib::ErrorCode::BufferUnderrun);
            }
            return StardustLib::Result<PingMessage>::success(msg);
        }
    );

    server.registerHandler<PingMessage>(MSG_TYPE_PING,
        [&server](uint64_t clientId, const PingMessage& msg) -> StardustLib::ErrorCode
        {
            PongMessage pong_response;
            pong_response.value = msg.value + 1;
            
            server.send(clientId, pong_response);
            return StardustLib::ErrorCode::Ok;
        }
    );

    // 6. Register a SERIALIZER for outgoing Pong messages
    server.registerSerializer<PongMessage>(MSG_TYPE_PONG,
        [](const PongMessage& msg, StardustLib::BufferWriter& writer)
        {
            writer.write<uint32_t>(msg.value);
        }
    );

    // 7. (Optional) Register a callback for when clients disconnect
    server.setDisconnectCallback([](uint64_t clientId)
    {
        std::cout << "Client " << clientId << " disconnected." << std::endl;
    });

    // 8. Start the server
    if (server.start())
    {
        std::cout << "Server started on port " << port << std::endl;
    }
    else
    {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    // 9. Keep the application running
    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();

    // 10. Stop the server
    std::cout << "Stopping server..." << std::endl;
    server.stop();
    std::cout << "Server stopped." << std::endl;

    return 0;
}
