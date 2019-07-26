#pragma once
#include <cstdint>
#include <WinSock2.h>

#define AGENT_MAX_MSGLEN  8192

inline uint32_t msglen(const void *p) {
    return 4 + ntohl(*(const uint32_t *)p);
}

void agent_query(void *buf);
