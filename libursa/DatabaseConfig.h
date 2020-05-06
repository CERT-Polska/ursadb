#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Json.h"

class ConfigKey {
    std::string key_;
    uint64_t defval_;

    explicit ConfigKey(std::string key, uint64_t defval)
        : key_(std::move(key)), defval_(defval) {}

   public:
    const std::string &key() const { return key_; }
    const uint64_t &defval() const { return defval_; }

    static std::optional<ConfigKey> parse(std::string_view value);

    const static std::vector<ConfigKey> available() {
        return {
            query_max_edge(),
            query_max_ngram(),
            database_workers(),
        };
    }

    const static ConfigKey query_max_edge() {
        return ConfigKey("query_max_edge", 1);
    }

    const static ConfigKey query_max_ngram() {
        return ConfigKey("query_max_ngram", 16);
    }

    const static ConfigKey database_workers() {
        return ConfigKey("database_workers", 4);
    }

    bool operator==(const ConfigKey &other) const { return other.key_ == key_; }
};

namespace std {
template <>
struct hash<ConfigKey> {
    size_t operator()(const ConfigKey &elm) const {
        return std::hash<std::string>()(elm.key());
    }
};
}  // namespace std

class DatabaseConfig {
    json config_;

   public:
    DatabaseConfig() : config_(std::unordered_map<std::string, uint64_t>()) {}
    explicit DatabaseConfig(json config);

    uint64_t get(const ConfigKey &key) const;
    void set(const ConfigKey &key, uint64_t value);
    std::unordered_map<std::string, uint64_t> get_all() const;
    const json get_raw() const { return config_; }
};
