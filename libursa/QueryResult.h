#pragma once

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "Core.h"

class QueryResult {
   private:
    std::vector<FileId> results;
    bool has_everything;

    QueryResult() : has_everything(true) {}
    QueryResult(const QueryResult &other) = delete;

   public:
    QueryResult(QueryResult &&other) = default;
    QueryResult(std::vector<FileId> results)
        : results(results), has_everything(false) {}
    QueryResult &operator=(QueryResult &&other) = default;

    static QueryResult empty() { return QueryResult(std::vector<FileId>()); }

    static QueryResult everything() { return QueryResult(); }

    void do_or(const QueryResult &other);

    void do_and(const QueryResult &other);

    // If true, means that QueryResults represents special
    // "uninitialized" value, "set of all FileIds in DataSet".
    bool is_everything() const { return has_everything; }

    const std::vector<FileId> &vector() const { return results; }
};
