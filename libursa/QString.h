#pragma once

#include <cstdint>
#include <vector>

// Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional
enum class QTokenType {
    CHAR = 1,       // normal byte e.g. \xAA
    WILDCARD = 2,   // full wildcard \x??
    HWILDCARD = 3,  // high wildcard e.g. \x?A
    LWILDCARD = 4   // low wildcard e.g. \xA?
};

class QToken {
    QTokenType legacy_type_;
    uint8_t legacy_val_;
    std::vector<uint8_t> opts_;

    QToken(const QToken &other) = delete;
    QToken(std::vector<uint8_t> &&opts, uint8_t val, QTokenType type)
        : opts_(std::move(opts)), legacy_type_(type), legacy_val_(val) {}

   public:
    QToken(QToken &&other) = default;
    static QToken single(uint8_t val);
    static QToken low_wildcard(uint8_t val);
    static QToken high_wildcard(uint8_t val);
    static QToken wildcard();

    std::vector<uint8_t> possible_values() const;
    bool operator==(const QToken &a) const;

    [[deprecated]] uint8_t val() const { return legacy_val_; }

    [[deprecated]] QTokenType type() const { return legacy_type_; }
};

// TODO change this typedef to a class?
using QString = std::vector<QToken>;
