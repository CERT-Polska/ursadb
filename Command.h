#pragma once

#include <variant>

#include "Query.h"

static inline std::vector<IndexType> default_index_types() {
    return {IndexType::GRAM3};
}

class SelectCommand {
    Query query;

  public:
    SelectCommand(const Query &query) : query(query) {}
    const Query &get_query() const { return query; }
};

class IndexCommand {
    std::vector<std::string> paths;
    std::vector<IndexType> types;

public:
    IndexCommand(const std::vector<std::string> &paths, const std::vector<IndexType> &types)
            : paths(paths), types(types) {}
    const std::vector<std::string> &get_paths() const { return paths; }
    const std::vector<IndexType> &get_index_types() const { return types; }
};

class IndexFromCommand {
    std::string path_list_fname;
    std::vector<IndexType> types;

public:
    IndexFromCommand(const std::string &path_list_fname, const std::vector<IndexType> &types)
            : path_list_fname(path_list_fname), types(types) {}
    const std::string &get_path_list_fname() const { return path_list_fname; }
    const std::vector<IndexType> &get_index_types() const { return types; }
};

class ReindexCommand {
    std::string dataset_name;
    std::vector<IndexType> types;

  public:
    ReindexCommand(const std::string &dataset_name, const std::vector<IndexType> &types)
            : dataset_name(dataset_name), types(types) {}
    const std::string &get_dataset_name() const { return dataset_name; }
    const std::vector<IndexType> &get_index_types() const { return types; }
};

enum CompactType { All = 1, Smart = 2 };

class CompactCommand {
    CompactType type;

  public:
    CompactCommand(CompactType type) : type(type) {}
    const CompactType get_type() const { return type; }
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

using Command = std::variant<
        SelectCommand, IndexCommand, IndexFromCommand, ReindexCommand, CompactCommand, StatusCommand, TopologyCommand, PingCommand>;