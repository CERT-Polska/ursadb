#include "QString.h"

#include <stdexcept>

QToken QToken::single(uint8_t val) { return QToken({val}); }

QToken QToken::low_wildcard(uint8_t val) {
    std::vector<uint8_t> options;
    if ((val & 0x0F) != 0) {
        throw std::runtime_error("format of low_wildcard is be 0xX0");
    }
    options.reserve(16);
    for (int i = 0; i < 16; i++) {
        options.push_back(static_cast<uint8_t>(val | i));
    }
    return QToken(std::move(options));
}

QToken QToken::high_wildcard(uint8_t val) {
    std::vector<uint8_t> options;
    if ((val & 0xF0) != 0) {
        throw std::runtime_error("format of high_wildcard is 0x0X");
    }
    options.reserve(16);
    for (int i = 0; i < 16; i++) {
        options.push_back((static_cast<uint8_t>(i << 4) | val));
    }
    return QToken(std::move(options));
}

QToken QToken::wildcard() {
    std::vector<uint8_t> options;
    options.reserve(256);
    for (int i = 0; i < 256; i++) {
        options.push_back(static_cast<uint8_t>(i));
    }
    return QToken(std::move(options));
}

QToken QToken::with_values(std::vector<uint8_t> &&values) {
    return QToken(std::move(values));
}

const std::vector<uint8_t> &QToken::possible_values() const { return opts_; }

uint64_t QToken::num_possible_values() const { return opts_.size(); }

bool QToken::operator==(const QToken &other) const {
    return opts_ == other.opts_;
}
