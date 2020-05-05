#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>

#include "Core.h"
#include "QueryCounters.h"

class QueryResult {
   private:
    std::vector<FileId> results;
    bool has_everything;

    QueryResult() : results{}, has_everything{true} {}
    QueryResult(const QueryResult &other) = default;

    // Number of explicitly stored files. This will return 0 for
    // QueryResult::everything.
    size_t file_count() const { return results.size(); }

    void do_or_real(const std::vector<FileId> &other);
    void do_and_real(const std::vector<FileId> &other);

    static QueryResult do_min_of_real(
        int cutoff, const std::vector<const QueryResult *> &sources);

   public:
    QueryResult(QueryResult &&other) = default;
    explicit QueryResult(std::vector<FileId> &&results)
        : results(results), has_everything(false) {}
    QueryResult &operator=(QueryResult &&other) = default;

    static QueryResult empty() { return QueryResult(std::vector<FileId>()); }

    static QueryResult everything() { return QueryResult(); }

    void do_or(const QueryResult &other, QueryCounter *counter);
    void do_and(const QueryResult &other, QueryCounter *counter);

    static QueryResult do_min_of(
        int cutoff, const std::vector<const QueryResult *> &sources,
        QueryCounter *counter);

    // If true, means that QueryResults represents special "uninitialized"
    // value, "set of all FileIds in DataSet".
    bool is_everything() const { return has_everything; }

    // If true, means that QueryResults is empty. This is useful for short
    // circuiting in some optimisations.
    bool is_empty() const { return !has_everything && results.empty(); }

    const std::vector<FileId> &vector() const { return results; }

    // For when you really need to clone the query result
    QueryResult clone() const { return *this; }
};

std::vector<FileId> internal_pick_common(
    int cutoff, const std::vector<const std::vector<FileId> *> &sources);
