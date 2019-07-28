#include "common.h"

namespace {

const char* const HexSymbols = "0123456789ABCDEF";

}  // anoynmous namespace

std::mutex _LogMutex;

std::ostream& operator <<(std::ostream& os, HexBuffer buff) {
    for (size_t i = 0; i < buff.len; ++i) {
        const uint8_t byte = reinterpret_cast<const uint8_t*>(buff.ptr)[i];
        os << HexSymbols[byte >> 4] << HexSymbols[byte & 0x0F];
    }
    return os;
}
