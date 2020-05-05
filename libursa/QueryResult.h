#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>

#include "Core.h"

// Counter utility class, for measuring a single aspect of performance.
class QueryCounter {
    // How many times was the operation performed? For aggregated counters.
    uint32_t count_;

    // How long did the operation take?
    std::chrono::steady_clock::duration duration_;

   public:
    QueryCounter() : count_{}, duration_{} {}

    QueryCounter(uint32_t count, std::chrono::steady_clock::duration duration)
        : count_(count), duration_(duration) {}

    uint32_t count() const { return count_; }
    uint64_t duration_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration_)
            .count();
    }

    void add(const QueryCounter &other);
};

class QueryOperation {
    QueryCounter *parent;
    std::chrono::steady_clock::time_point start_;

   public:
    QueryOperation(QueryCounter *parent)
        : parent(parent), start_{std::chrono::steady_clock::now()} {}

    ~QueryOperation();
};

// Container for various query performance counters and statistics.
class QueryCounters {
    // Counter for `and` operations.
    QueryCounter ands_;

    // Counter for `or` operations.
    QueryCounter ors_;

    // Counter for file reads.
    QueryCounter reads_;

    // Counter for min ... of operations.
    QueryCounter minofs_;

   public:
    QueryCounters() : reads_{} {}

    QueryCounter &ands() { return ands_; }

    QueryCounter &ors() { return ors_; }

    QueryCounter &reads() { return reads_; }

    QueryCounter &minofs() { return minofs_; }

    void add(const QueryCounters &other);

    // Create a map with all counters (for serialisation).
    std::unordered_map<std::string, QueryCounter> counters() const;
};

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
