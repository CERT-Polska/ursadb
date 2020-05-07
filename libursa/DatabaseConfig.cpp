#include "DatabaseConfig.h"

#include "spdlog/spdlog.h"

DatabaseConfig::DatabaseConfig(json config) : config_(config) {
    for (const auto &elm : config.items()) {
        if (ConfigKey::parse(elm.key())) {
            spdlog::info("CONFIG: {}={}", elm.key(), elm.value().dump(4));
        } else {
            spdlog::warn("Unexpected config: {}={}", elm.key(),
                         elm.value().dump(4));
        }
    }
}

std::optional<ConfigKey> ConfigKey::parse(std::string_view value) {
    for (const auto &opt : ConfigKey::available()) {
        if (opt.key() == value) {
            return opt;
        }
    }
    return std::nullopt;
}

uint64_t DatabaseConfig::get(const ConfigKey &key) const {
    if (const auto &it = config_.find(key.key()); it != config_.end()) {
        return *it;
    }
    return key.defval();
}

bool DatabaseConfig::can_set(const ConfigKey &key, uint64_t value) const {
    if (value < key.min()) {
        return false;
    }
    if (value > key.max()) {
        return false;
    }
    return true;
}

void DatabaseConfig::set(const ConfigKey &key, uint64_t value) {
    if (!can_set(key, value)) {
        throw std::runtime_error("Tried to set config to invalid value");
    }
    config_[key.key()] = value;
}

std::unordered_map<std::string, uint64_t> DatabaseConfig::get_all() const {
    std::unordered_map<std::string, uint64_t> result;
    for (const auto &key : ConfigKey::available()) {
        result.emplace(key.key(), get(key));
    }
    return result;
}
