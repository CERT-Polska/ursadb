#include "QString.h"

#include <stdexcept>

QToken QToken::single(uint8_t val) {
    return QToken({val}, val, QTokenType::CHAR);
}

QToken QToken::low_wildcard(uint8_t val) {
    std::vector<uint8_t> options;
    if ((val & 0x0F) != 0) {
        throw std::runtime_error("format of low_wildcard is be 0xX0");
    }
    for (int i = 0; i < 16; i++) {
        options.push_back(static_cast<uint8_t>(val | i));
    }
    return QToken(std::move(options), val, QTokenType::LWILDCARD);
}

QToken QToken::high_wildcard(uint8_t val) {
    std::vector<uint8_t> options;
    if ((val & 0xF0) != 0) {
        throw std::runtime_error("format of high_wildcard is 0x0X");
    }
    for (int i = 0; i < 16; i++) {
        options.push_back((static_cast<uint8_t>(i << 4) | val));
    }
    return QToken(std::move(options), val, QTokenType::HWILDCARD);
}

QToken QToken::wildcard() {
    std::vector<uint8_t> options;
    for (int i = 0; i < 256; i++) {
        options.push_back(static_cast<uint8_t>(i));
    }
    return QToken(std::move(options), 0, QTokenType::WILDCARD);
}

std::vector<uint8_t> QToken::possible_values() const { return opts_; }

bool QToken::operator==(const QToken &other) const {
    return opts_ == other.opts_;
}
