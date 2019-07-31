#pragma once
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifndef NDEBUG
    #include <mutex>
#endif

#ifdef NDEBUG
    #define LOG_DEBUG(msg)
#else
    extern std::mutex _DbgLogMutex;
    #define LOG_DEBUG(msg) do { \
        std::lock_guard<std::mutex> lock(_DbgLogMutex); \
        std::cout << "[DBG] " << msg << std::endl; \
    } while(false)
#endif

#define LOG_ERROR(msg) try { \
    std::cerr << "[ERR] " << msg << std::endl; \
} catch(...) { }

#define THROW_RUNTIME_ERROR(msg) do { \
    std::ostringstream os; os << msg; \
    throw std::runtime_error(os.str()); \
} while(false)


struct Buffer {
    void* const ptr = nullptr;
    size_t len = 0;

    Buffer() = default;
    Buffer(void* p, size_t l) : ptr(p), len(l) { }
};


// use it for writing binary data as a HEX stream
using HexBuffer = Buffer;

std::ostream& operator <<(std::ostream&, HexBuffer buff);


