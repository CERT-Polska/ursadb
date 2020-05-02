#include "DatabaseConfig.h"

#include "spdlog/spdlog.h"

DatabaseConfig::DatabaseConfig(json config) : config_(config) {
    defvals_ = {
        {"query_max_edge", 1},
        {"query_max_ngram", 16},
    };
    for (const auto &elm : config.items()) {
        if (defvals_.count(elm.key()) == 0) {
            spdlog::warn("Unexpected config: {}={}", elm.key(),
                         elm.value().dump(4));
        } else {
            spdlog::info("CONFIG: {}={}", elm.key(), elm.value().dump(4));
        }
    }
}

uint64_t DatabaseConfig::get(const ConfigKey &key) const {
    if (defvals_.count(key.key()) == 0) {
        throw std::runtime_error("Invalid config key (get)");
    }
    if (const auto &it = config_.find(key.key()); it != config_.end()) {
        return *it;
    }
    return defvals_.at(key.key());
}

void DatabaseConfig::set(const ConfigKey &key, uint64_t value) {
    if (defvals_.count(key.key()) == 0) {
        throw std::runtime_error("Invalid config key (set)");
    }
    config_[key.key()] = value;
}

std::unordered_map<std::string, uint64_t> DatabaseConfig::get_all() const {
    std::unordered_map<std::string, uint64_t> result;
    for (const auto &[k, v] : defvals_) {
        result.emplace(k, get(k));
    }
    return result;
}
