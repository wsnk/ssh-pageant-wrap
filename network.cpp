#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <thread>
#include <atomic>
#include <sstream>
#include <functional>

#include "network.h"
#include "common.h"
#include <winsock2.h>

#include <Windows.h>
#include <Winbase.h>

#define LOCALHOST_ADDR "127.0.0.1"
#define TCP_PORT 51515

namespace {

char buff[BUFF_SIZE];

int Recv(SOCKET sock, char* dst, int len, int flags = 0) {
    assert(len > 0);
    int rc = recv(sock, dst, len, flags);
    if (rc == SOCKET_ERROR)
        THROW_RUNTIME_ERROR("socket recv failed: " << WSAGetLastError());
    return rc;
}

int RecvExact(SOCKET sock, char* dst, unsigned len) {
    return Recv(sock, dst, len, MSG_WAITALL);
}


// returns length of ssh-agent protocol message
uint32_t SaMessageLen(const void *p) {
    return ntohl(*reinterpret_cast<const uint32_t*>(p));
}

// receive ssh-agent protocol message
int RecvSaMessage(SOCKET sock, char* dst, unsigned len) {
    assert(len > sizeof(uint32_t));

    int recvLen = RecvExact(sock, dst, sizeof(uint32_t));
    if (recvLen == 0)
        return 0;

    const uint32_t msgLen = SaMessageLen(dst);
    if (msgLen > len - sizeof(uint32_t))
        THROW_RUNTIME_ERROR("sizeof of SA message is too big: " << msgLen);

    recvLen += RecvExact(sock, dst + sizeof(uint32_t), msgLen);
    return recvLen;
}


void ProcessClient(SOCKET sock, Network::Handler handler)
{
    unsigned counter = 0;
    while (true) {
        int len = counter > 1
                ? RecvSaMessage(sock, buff, sizeof(buff))
                : Recv(sock, buff, sizeof(buff));  // first 2 messages are not about ssh-agent protocol

        if (len == 0)
            return;  // socket is closed by remote side

        LOG_DEBUG("Request of " << len << " bytes:\n" << HexBuffer(buff, len));
        if (counter > 1) {
            len = handler(buff, len);
        }
        LOG_DEBUG("Response of " << len << " bytes:\n" << HexBuffer(buff, len));

        send(sock, buff, len, 0);
        ++counter;
    }

    shutdown(sock, SD_BOTH);
    closesocket(sock);

    LOG_DEBUG("Socket closed");
}

void Run(SOCKET sock, Network::Handler handler, std::atomic_flag& runFlag)
{
    try {
        LOG_DEBUG("Socket thread is running...");
        while (runFlag.test_and_set()) {
            sockaddr_in addr;
            int addrLen = sizeof(addr);

            LOG_DEBUG("Accept new connection");
            SOCKET remote_sock = accept(sock, (sockaddr*)&addr, &addrLen);  // it's blocking and runFlag doesn't work actually for now
            if (remote_sock == INVALID_SOCKET) {
                if (!runFlag.test_and_set()) {
                    LOG_DEBUG("Socket accept interrupted via WSACleanup()");
                    break;
                }
                LOG_ERROR("Socket failed on accept: " << WSAGetLastError());
                continue;
            }

            LOG_DEBUG("Socket accepted connection from " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port));
            try {
                ProcessClient(remote_sock, handler);
                LOG_DEBUG("Connection successfully handled and closed");
            } catch (const std::exception& exc) {
                LOG_ERROR("Error in processing connection: "  << exc.what());
            }
        }

        LOG_DEBUG("Socket thread is finished");
    } catch (const std::exception& exc) {
        LOG_ERROR("Socket thread epically crashed: " << exc.what());
    } catch (...) {
        LOG_ERROR("Socket thread epically crashed: unknown exception");
    }

    // actually socket is closed by WSACleanup() call
    shutdown(sock, SD_BOTH);
    closesocket(sock);

    LOG_DEBUG("Socket is closed");
}

int start_network(SOCKET& sock) {
    WSADATA wsaData = {0};
    if (int err = WSAStartup(MAKEWORD(2, 2), &wsaData))
        THROW_RUNTIME_ERROR("WSAStartup failed: " << err);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        THROW_RUNTIME_ERROR("socket failed: " << WSAGetLastError());

    LOG_DEBUG("Socket created");

    sockaddr_in addr;
    int addrlen = sizeof(addr);
    addr.sin_family             = AF_INET;
    addr.sin_addr.S_un.S_addr   = inet_addr("127.0.0.1");
    addr.sin_port               = 0;

    if (bind(sock, (const sockaddr*)&addr, addrlen) == SOCKET_ERROR)
        THROW_RUNTIME_ERROR("socket binding failed: " << WSAGetLastError());

    getsockname(sock, (sockaddr*)&addr, &addrlen);
    LOG_DEBUG("Socket binded to " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port));

    if (listen(sock, 50) == SOCKET_ERROR)
        THROW_RUNTIME_ERROR("socket listening failed: " << WSAGetLastError());

    LOG_DEBUG("Socket is listening");
    return ntohs(addr.sin_port);
}


// global variables, sorry
SOCKET Sock = INVALID_SOCKET;
std::atomic_flag RunFlag;

}  // anoynmous namespace


Network::Network(Handler handler)
{
    Port = start_network(Sock);

    RunFlag.test_and_set();
    Thread = std::thread(&Run, Sock, handler, std::ref(RunFlag));
}

Network::~Network()
{
    RunFlag.clear();
    WSACleanup();  // it'll interrupt blocking call in Thread

    try { Thread.join(); } catch (...) { }
}


//------------------------------------------------------------------------------


namespace {

std::string CreateSocketDirectory() {
    const std::string dirName("/ssh-9Aue8UISfBOA");
    const std::string fullDirName = std::getenv("TEMP") + dirName;

    DWORD attrs = GetFileAttributesA(fullDirName.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if (!CreateDirectoryA(fullDirName.c_str(), NULL))
            throw std::runtime_error("couldn't create socket directory");
    }

    return dirName;
}

const char* const ENV_VAR_NAME = "SSH_AUTH_SOCK";



}  // anonymous namespace

FakeSocketFile::FakeSocketFile(uint16_t port)
{
    const std::string fileName = std::string("/agent.") + std::to_string(GetCurrentProcessId());
    const std::string dirName = CreateSocketDirectory();
    const std::string fullDirName = std::string(getenv("TEMP")) + dirName;

    std::ostringstream os;
    os << fullDirName << fileName;
    FileName = os.str();

    {
        std::remove(FileName.c_str());
        std::FILE* f = std::fopen(FileName.c_str(), "wb");
        if (!f)
            THROW_RUNTIME_ERROR("socket file creation failed: ");

        os = std::ostringstream();
        os << "!<socket >" << port << " s F7587A34-12288B3F-DFF3A077-C4B57B4";
        const std::string& str = os.str();

        std::fwrite(str.c_str(), str.length() + 1, 1, f); // with null termination
        std::fclose(f);

        LOG_DEBUG("Socket file created: " << FileName <<"; content: " << str);

        if (!SetFileAttributesA(FileName.c_str(), FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_SYSTEM)) {
            LOG_ERROR("couldn't set attributes");
        }
    }

    const std::string envValue = std::string("/tmp") + dirName + fileName;
    if (!SetEnvironmentVariableA(ENV_VAR_NAME, envValue.c_str())) {
        LOG_ERROR("Couldn't set end variable: " << GetLastError());
    }
    LOG_DEBUG("Environment variable set: " << ENV_VAR_NAME << "=" << envValue);
}

FakeSocketFile::~FakeSocketFile()
{
    if (int rc = std::remove(FileName.c_str()))
        LOG_ERROR("couldn't remove socket file " << FileName);
    LOG_DEBUG("Socket file " << FileName << " removed");
    SetEnvironmentVariableA(ENV_VAR_NAME, NULL);
}
