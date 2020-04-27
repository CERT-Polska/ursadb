#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "Json.h"

class ConfigKey {
    const std::string key_;

   public:
    ConfigKey(std::string key) : key_(key) {}
    const std::string &key() const { return key_; }
};

namespace config {
const ConfigKey query_max_edge = ConfigKey("query_max_edge");
const ConfigKey query_max_ngram = ConfigKey("query_max_ngram");
}  // namespace config

class DatabaseConfig {
    json config_;
    std::unordered_map<std::string, uint64_t> defvals_;

   public:
    DatabaseConfig() : config_(std::unordered_map<std::string, uint64_t>()) {}
    explicit DatabaseConfig(json config);

    uint64_t get(const ConfigKey &key) const;
    void set(const ConfigKey &key, uint64_t value);
    std::unordered_map<std::string, uint64_t> get_all() const;
    const json get_raw() const { return config_; }
};
