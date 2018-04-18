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

  public:
    IndexCommand(const std::string &path) : path(path) {}
    const std::string &get_path() const { return path; }
};

using Command = std::variant<SelectCommand, IndexCommand>;