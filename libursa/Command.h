#pragma once

#include <set>
#include <variant>

#include "Core.h"
#include "Query.h"

static inline std::vector<IndexType> default_index_types() {
    return {IndexType::GRAM3};
}

class SelectCommand {
    Query query_;
    std::set<std::string> taints_;
    std::set<std::string> datasets_;
    bool use_iterator_;

   public:
    SelectCommand(Query &&query, std::set<std::string> taints,
                  std::set<std::string> datasets, bool use_iterator)
        : query_(std::move(query)),
          taints_(std::move(taints)),
          datasets_(std::move(datasets)),
          use_iterator_(use_iterator) {}
    const Query &get_query() const { return query_; }
    const std::set<std::string> &taints() const { return taints_; }
    const std::set<std::string> &datasets() const { return datasets_; }
    const bool iterator_requested() const { return use_iterator_; }
};

class IndexCommand {
    std::vector<std::string> paths_;
    std::vector<IndexType> types_;
    bool ensure_unique_;

   public:
    IndexCommand(const std::vector<std::string> &paths,
                 const std::vector<IndexType> &types, bool ensure_unique)
        : paths_(paths), types_(types), ensure_unique_(ensure_unique) {}
    const std::vector<std::string> &get_paths() const { return paths_; }
    const std::vector<IndexType> &get_index_types() const { return types_; }
    const bool ensure_unique() const { return ensure_unique_; }
};

class IteratorPopCommand {
    std::string iterator_id;
    uint64_t how_many;

   public:
    IteratorPopCommand(const std::string &iterator_id, uint64_t how_many)
        : iterator_id(iterator_id), how_many(how_many) {}
    const std::string &get_iterator_id() const { return iterator_id; }
    uint64_t elements_to_pop() const { return how_many; }
};

class IndexFromCommand {
    std::string path_list_fname_;
    std::vector<IndexType> types_;
    bool ensure_unique_;

   public:
    IndexFromCommand(const std::string &path_list_fname,
                     const std::vector<IndexType> &types, bool ensure_unique)
        : path_list_fname_(path_list_fname),
          types_(types),
          ensure_unique_(ensure_unique) {}
    const std::string &get_path_list_fname() const { return path_list_fname_; }
    const std::vector<IndexType> &get_index_types() const { return types_; }
    const bool ensure_unique() const { return ensure_unique_; }
};

class ReindexCommand {
    std::string dataset_id_;
    std::vector<IndexType> types_;

   public:
    ReindexCommand(const std::string &dataset_id,
                   const std::vector<IndexType> &types)
        : dataset_id_(dataset_id), types_(types) {}
    const std::string &dataset_id() const { return dataset_id_; }
    const std::vector<IndexType> &get_index_types() const { return types_; }
};

enum CompactType { All = 1, Smart = 2 };

class CompactCommand {
    CompactType type;

   public:
    CompactCommand(CompactType type) : type(type) {}
    const CompactType get_type() const { return type; }
};

class ConfigGetCommand {
    std::vector<std::string> keys_;

   public:
    ConfigGetCommand(const std::vector<std::string> &keys) : keys_(keys) {}
    const std::vector<std::string> &keys() const { return keys_; }
};

class ConfigSetCommand {
    std::string key_;
    uint64_t value_;

   public:
    ConfigSetCommand(const std::string &key, uint64_t value)
        : key_(key), value_(value) {}
    const std::string &key() const { return key_; }
    const uint64_t &value() const { return value_; }
};

class StatusCommand {
   public:
    StatusCommand() {}
};

class TopologyCommand {
   public:
    TopologyCommand() {}
};

class PingCommand {
   public:
    PingCommand() {}
};

enum class TaintMode { Add = 1, Clear = 2 };

class TaintCommand {
    std::string dataset;
    TaintMode mode;
    std::string taint;

   public:
    TaintCommand(std::string dataset, TaintMode mode, std::string taint)
        : dataset(dataset), mode(mode), taint(taint) {}

    const TaintMode get_mode() const { return mode; }

    const std::string &get_dataset() const { return dataset; }

    const std::string &get_taint() const { return taint; }
};

using Command = std::variant<SelectCommand, IndexCommand, IndexFromCommand,
                             IteratorPopCommand, ReindexCommand, CompactCommand,
                             ConfigGetCommand, ConfigSetCommand, StatusCommand,
                             TopologyCommand, PingCommand, TaintCommand>;
