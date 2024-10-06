#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>

#include "Core.h"
#include "QueryCounters.h"
#include "SortedRun.h"

class QueryResult {
   private:
    SortedRun results;
    bool has_everything;

    QueryResult() : results{}, has_everything{true} {}

    static QueryResult do_min_of_real(int cutoff,
                                      std::vector<QueryResult *> &sources);

   public:
    QueryResult(QueryResult &&other) = default;
    explicit QueryResult(SortedRun &&results)
        : results(std::move(results)), has_everything(false) {}
    QueryResult &operator=(QueryResult &&other) = default;

    static QueryResult empty() { return QueryResult(SortedRun()); }

    static QueryResult everything() { return QueryResult(); }

    void do_or(QueryResult &&other, QueryCounter *counter);
    void do_and(QueryResult &&other, QueryCounter *counter);

    static QueryResult do_min_of(int cutoff,
                                 std::vector<QueryResult *> &sources,
                                 QueryCounter *counter);

    // If true, means that QueryResults represents special "uninitialized"
    // value, "set of all FileIds in DataSet".
    bool is_everything() const { return has_everything; }

    // If true, means that QueryResults is empty. This is useful for short
    // circuiting in some optimisations.
    bool is_empty() const { return !has_everything && results.empty(); }

    const SortedRun &vector() const { return results; }
    SortedRun &vector() { return results; }
};
