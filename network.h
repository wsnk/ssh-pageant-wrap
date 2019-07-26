#pragma once
#include <cstdint>
#include <string>
#include <thread>

#define BUFF_SIZE 16384  // 16Kb should be enough for everyone

// must be the only one instance (singletone)
class Network {
public:
    using Handler = int(*)(char* buff, int len);

public:
    Network(Handler handler);
    ~Network();

    uint16_t GetPort() const { return Port; }

private:
    uint16_t Port;
    std::thread Thread;
};


class FakeSocketFile {
public:
    FakeSocketFile(uint16_t port);
    ~FakeSocketFile();

private:
    std::string FileName;
};

