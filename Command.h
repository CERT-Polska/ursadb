#pragma once

#include <variant>

#include "Query.h"

class SelectCommand {
    Query query;

  public:
    SelectCommand(const Query &query) : query(query) {}
    const Query &get_query() const { return query; }
};

class IndexCommand {
    std::string path;
    std::vector<IndexType> types;

  public:
    IndexCommand(const std::string &path, const std::vector<IndexType> &types)
        : path(path), types(types) {}
    const std::string &get_path() const { return path; }
    const std::vector<IndexType> &get_index_types() const { return types; }

    static std::vector<IndexType> default_types() { return {IndexType::GRAM3}; }
};

class CompactCommand {
  public:
    CompactCommand() {}
};

class StatusCommand {
  public:
    StatusCommand() {}
};

class TopologyCommand {
  public:
    TopologyCommand() {}
};

using Command =
        std::variant<SelectCommand, IndexCommand, CompactCommand, StatusCommand, TopologyCommand>;