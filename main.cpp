#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <mutex>

#include "network.h"
#include "pageant.h"
#include "common.h"

#include <Windows.h>
#include <Winbase.h>


const char* const ssh = GIT_SSH_PATH;
Pageant pageant;
std::mutex pageantMtx;

int SendToAgent(char* buff, int len)
{
    Buffer msg(buff, len);
    {
        std::lock_guard<std::mutex> lock(pageantMtx);
        pageant.Query(msg);
    }
    return msg.len;
}

int main(int argc, char* argv[])
{
    try {
        {
            Network net(&SendToAgent);
            FakeSocketFile sFile(net.GetPort());

            // below we just launch ssh

            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = NULL;
            sa.bInheritHandle = TRUE;

            STARTUPINFOA si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));

            si.cb = sizeof(si);
            si.hStdInput    = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput   = GetStdHandle(STD_OUTPUT_HANDLE);
            si.hStdError    = GetStdHandle(STD_ERROR_HANDLE);

            char* const commandLine = GetCommandLineA();

            if (!CreateProcessA(
                ssh,
                commandLine,
                &sa,
                &sa,
                TRUE,
                0,
                NULL,
                NULL,
                &si,
                &pi
            )) {
                LOG_ERROR("Couldn't create process:\n  " << ssh << ' ' << commandLine
                          << "\nError " << GetLastError());
                return -1;
            }

            LOG_DEBUG("Waiting for child process...");
            WaitForSingleObject( pi.hProcess, INFINITE);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        return GetLastError();
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << std::endl;
    }
}



