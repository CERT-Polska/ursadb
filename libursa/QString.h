#pragma once

#include <cstdint>
#include <vector>

// Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional
enum class QTokenType {
    CHAR = 1,       // normal byte e.g. \xAA
    WILDCARD = 2,   // full wildcard \x??
    HWILDCARD = 3,  // high wildcard e.g. \x?A
    LWILDCARD = 4,  // low wildcard e.g. \xA?
    EXPLICIT = 5    // explicit option list. Not understood by legacy methods.
};

// Represents a single token in a query. For example AA, A?, ?? or (AA | BB).
class QToken {
    // Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional.
    QTokenType legacy_type_;

    // Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional.
    uint8_t legacy_val_;

    // List of possible options for this token. Between 1 and 256 byte values.
    // Should be sorted in the ascending order, otherwise == won't work.
    std::vector<uint8_t> opts_;

    QToken(const QToken &other) = default;
    QToken(std::vector<uint8_t> &&opts, uint8_t val, QTokenType type)
        : legacy_type_(type), legacy_val_(val), opts_(std::move(opts)) {}

   public:
    QToken(QToken &&other) = default;

    // Creates a token with only one possible value.
    static QToken single(uint8_t val);

    // Creates a token in a form {X?} (for example 1?, 2?, 3?...).
    static QToken low_wildcard(uint8_t val);

    // Creates a token in a form {?X} (for example ?1, ?2, ?3...).
    static QToken high_wildcard(uint8_t val);

    // Creates a wildcard token ({??}).
    static QToken wildcard();

    // Creates a wildcard with exactly given option list.
    static QToken with_values(std::vector<uint8_t> &&values);

    // Returns a list of possible values for this token.
    const std::vector<uint8_t> &possible_values() const;

    // Returns a number of possible values for this token.
    // Equivalent to `possible_values.size()`.
    uint64_t num_possible_values() const;

    // Returns true, if the QToken is empty (doesn't accept any character).
    bool empty() const { return opts_.empty(); }

    // Compares two QTokens. Assumes that `opts_` is in the ascending order.
    bool operator==(const QToken &a) const;

    // Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional.
    // Returns a masked token value, exact meaning depends on the token type.
    [[deprecated]] uint8_t val() const { return legacy_val_; }

    // Keep this only as long as EXPERIMENTAL_QUERY_GRAPHS feature is optional.
    // Returns a token type.
    [[deprecated]] QTokenType type() const { return legacy_type_; }

    // For when you really positively need to use a copy constructor.
    QToken clone() const { return QToken(*this); }
};

// Represents a query string, sequence of tokens. For example {11 22 ?3 44}.
// TODO change this typedef to a class?
using QString = std::vector<QToken>;
