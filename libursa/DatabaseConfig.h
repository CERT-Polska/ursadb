#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Json.h"

class ConfigKey {
    // Key name, as used by `config` commands.
    std::string key_;

    // Default value, used when the key was not configured explicitly.
    uint64_t defval_;

    // Minimum allowed value for this key.
    uint64_t min_;

    // Maximum allowed value for this key.
    uint64_t max_;

    // Creates a new ConfigKey instance with the specified settings.
    explicit ConfigKey(std::string key, uint64_t defval, uint64_t min,
                       uint64_t max)
        : key_(std::move(key)), defval_(defval), min_(min), max_(max) {}

   public:
    const std::string &key() const { return key_; }
    uint64_t defval() const { return defval_; }
    uint64_t min() const { return min_; }
    uint64_t max() const { return max_; }

    static std::optional<ConfigKey> parse(std::string_view value);

    // Returns a list of supported config keys.
    static std::vector<ConfigKey> available() {
        return {
            query_max_edge(),     query_max_ngram(), database_workers(),
            merge_max_datasets(), merge_max_files(),
        };
    }

    // Maximum number of values a first or last character in sequence can take
    // to be considered when planning a query. The default is a conservative 1,
    // so query plam will never start or end with a wildcard.
    // Recommendation: Stick to the default value. If you have a good disk and
    // want to reduce false-positives, increase to 16.
    const static ConfigKey query_max_edge() {
        return ConfigKey("query_max_edge", 1, 1, 254);
    }

    // Maximum number of values a ngram can take to be considered when planning
    // a query. For example, with a default value of 16, trigram `11 2? 33` will
    // be expanded and included in query, but `11 ?? 33` will be ignored.
    // Recommendation: Stick to the default value at first. If your queries are
    // fast, use many wildcards, but have many false positives, increase to 256.
    const static ConfigKey query_max_ngram() {
        return ConfigKey("query_max_ngram", 16, 1, 16777216);
    }

    // How many tasks can be processed at once? The default 4 is a very
    // conservative value for most workloads. Increasing it will make the
    // database faster, but at a certain point the disk becomes a bottleneck.
    // Recommendation: If your server is dedicated to ursadb, or your IO latency
    // is high (for example, files are stored on NFS), increase to at least 16.
    const static ConfigKey database_workers() {
        return ConfigKey("database_workers", 4, 1, 1024);
    }

    // How many datasets can be merged at once? This has severe memory usage
    // implications - for merging datasets must be fully loaded, and every
    // loaded dataset consumes a bit over 128MiB. Increasing this number makes
    // compacting huge datasets faster, but may run out of ram.
    // Recommendation: merge_max_datasets * 128MiB can safely be set to around
    // 1/4 of RAM dedicated to the database, so for example 8 for 4GiB server
    // or 32 for 16GiB server.
    const static ConfigKey merge_max_datasets() {
        return ConfigKey("merge_max_datasets", 10, 1, 1024);
    }

    // When merging, what is the maximal allowed number of files in the
    // resulting dataset? Large datasets make the database faster, but also need
    // more memory to run efficiently.
    // Recommendation: ursadb was used with multi-million datasets in the wild,
    // but currently we recommend to stay on the safe side and don't create
    // datasets larger than 1 million files.
    const static ConfigKey merge_max_files() {
        return ConfigKey("merge_max_files", 1024 * 1024 * 1024, 1, 4294967295);
    }

    // Compares two ConfigKey instances for equality.
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
    // Constructs an empty config instance.
    DatabaseConfig() : config_(std::unordered_map<std::string, uint64_t>()) {}

    // Constructs a new instance using given json.
    explicit DatabaseConfig(json config);

    // Gets the current value of a given key, or returns a default.
    uint64_t get(const ConfigKey &key) const;

    // Checks is it possible to set key to a given value.
    bool can_set(const ConfigKey &key, uint64_t value) const;

    // Sets the key to the given value, or throws an exception if it's invalid.
    void set(const ConfigKey &key, uint64_t value);

    // Gets all key-value pairs, including defaults.
    std::unordered_map<std::string, uint64_t> get_all() const;

    // Gets internal configuration JSON.
    const json get_raw() const { return config_; }
};
