#include "Core.h"

#include <stdexcept>

std::vector<uint8_t> QToken::possible_values() const {
    std::vector<uint8_t> options;
    switch (type_) {
        case QTokenType::CHAR:
            options.push_back(val_);
            break;
        case QTokenType::WILDCARD:
            for (int i = 0; i < 256; i++) {
                options.push_back(static_cast<uint8_t>(i));
            }
            break;
        case QTokenType::HWILDCARD:
            for (int i = 0; i < 16; i++) {
                options.push_back(static_cast<uint8_t>((i << 4) | val_));
            }
            break;
        case QTokenType::LWILDCARD:
            for (int i = 0; i < 16; i++) {
                options.push_back(static_cast<uint8_t>(val_ | i));
            }
            break;
        default:
            throw std::runtime_error("Invalid QToken type");
    }
    return options;
}
