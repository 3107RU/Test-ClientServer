#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace netlib
{
    // Server port
    static const int SERVER_PORT = 3107;

    struct Msg
    {
#pragma pack(push)
#pragma pack(1)
        struct Header
        {
            uint32_t idx;
            uint64_t time;
            uint16_t size;
            uint8_t md5[16];
        };
#pragma pack(pop)
        Header header;
        bool valid;
        std::vector<uint16_t> data;
    };

    using OnMsgFn = std::function<void(std::shared_ptr<Msg>)>;

    class Server
    {
        class Impl;
        std::shared_ptr<Impl> impl;

    public:
        Server(OnMsgFn onMsg);
    };

    class Client
    {
        class Impl;
        std::shared_ptr<Impl> impl;

    public:
        Client(const std::string &address);
        bool isConnected();
        bool send(std::shared_ptr<Msg> msg);
        void stop();
        bool isFinished();
    };
}