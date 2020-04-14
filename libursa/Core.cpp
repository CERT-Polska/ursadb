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

std::string get_index_type_name(IndexType type) {
    switch (type) {
        case IndexType::GRAM3:
            return "gram3";
        case IndexType::TEXT4:
            return "text4";
        case IndexType::HASH4:
            return "hash4";
        case IndexType::WIDE8:
            return "wide8";
    }

    throw std::runtime_error("unhandled index type");
}

std::optional<IndexType> index_type_from_string(const std::string &type) {
    if (type == "gram3") {
        return IndexType::GRAM3;
    } else if (type == "text4") {
        return IndexType::TEXT4;
    } else if (type == "hash4") {
        return IndexType::HASH4;
    } else if (type == "wide8") {
        return IndexType::WIDE8;
    } else {
        return std::nullopt;
    }
}
