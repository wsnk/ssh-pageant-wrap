#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "network.h"



int do_something(char* buff, int len)
{
    std::puts("I received:\n");

    buff[len] = 0;
    std::puts(buff);
    std::puts("\n");

    const char* resp = "Yeas, that's fine...\n";
    int resp_len = std::strlen(resp);

    std::memcpy(buff, resp, resp_len);
    return resp_len;
}

int main(int argc, char* argv[])
{
    int port = start_network(&do_something);
    std::cout << "Listen on " << port << std::endl;

    char c;
    std::cin >> c;

    stop_network();

    return 0;
}


