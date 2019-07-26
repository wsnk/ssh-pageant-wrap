#pragma once
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef NDEBUG
    #define LOG_DEBUG(msg)
#else
    #define LOG_DEBUG(msg) do { \
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


// use it for writing binary data as a HEX stream
struct HexBuffer {
    const void* ptr;
    size_t len;
    HexBuffer(const void* p, size_t l) : ptr(p), len(l) { }
};
std::ostream& operator <<(std::ostream&, HexBuffer buff);


