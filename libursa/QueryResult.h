#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>

#include "Core.h"

// Counter utility class, for measuring a single aspect of performance.
class QueryCounter {
    // How many times was the operation performed? For aggregated counters.
    uint32_t count_;

    // How many files did the operation touch?
    uint64_t in_files_;

    // How many files did the operation produce?
    uint64_t out_files_;

    // How long did the operation take?
    std::chrono::steady_clock::duration duration_;

   public:
    QueryCounter() : count_{}, in_files_{}, out_files_{}, duration_{} {}

    QueryCounter(uint32_t count, uint64_t in_files, uint64_t out_files,
                 std::chrono::steady_clock::duration duration)
        : count_(count),
          in_files_(in_files),
          out_files_(out_files),
          duration_(duration) {}

    uint32_t count() const { return count_; }
    uint64_t in_files() const { return in_files_; }
    uint64_t out_files() const { return out_files_; }
    uint64_t duration_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration_)
            .count();
    }

    void add(const QueryCounter &other);
};

class QueryOperation {
    uint32_t in_files_;
    std::chrono::steady_clock::time_point start_;

   public:
    QueryOperation(uint32_t in_files)
        : in_files_{in_files}, start_{std::chrono::steady_clock::now()} {}

    QueryCounter end(uint32_t out_files) const;
};

// Container for various query performance counters and statistics.
class QueryStatistics {
    // Counter for `and` operations.
    QueryCounter ands_;

    // Counter for `or` operations.
    QueryCounter ors_;

    // Counter for file reads.
    QueryCounter reads_;

   public:
    QueryStatistics() : ands_{}, ors_{}, reads_{} {}
    QueryStatistics(uint64_t file_count)
        : ands_{}, ors_{}, reads_{1, 0, file_count, {}} {}

    void add_and(QueryCounter counter) { ands_.add(counter); }

    void add_or(QueryCounter counter) { ors_.add(counter); }

    void add_read(QueryCounter counter) { reads_.add(counter); }

    void add(const QueryStatistics &other);

    // Create a map with all counters (for serialisation).
    std::unordered_map<std::string, QueryCounter> counters() const;
};

class QueryResult {
   private:
    std::vector<FileId> results;
    bool has_everything;
    QueryStatistics stats_;

    QueryResult() : results{}, has_everything{true}, stats_{} {}
    QueryResult(const QueryResult &other) = default;

    // Number of explicitly stored files. This will return 0 for
    // QueryResult::everything.
    size_t file_count() const { return results.size(); }

   public:
    QueryResult(QueryResult &&other) = default;
    explicit QueryResult(std::vector<FileId> &&results)
        : results(results), has_everything(false), stats_{results.size()} {}
    QueryResult &operator=(QueryResult &&other) = default;

    static QueryResult empty() { return QueryResult(std::vector<FileId>()); }

    static QueryResult everything() { return QueryResult(); }

    void do_or(const QueryResult &other);

    void do_and(const QueryResult &other);

    // If true, means that QueryResults represents special
    // "uninitialized" value, "set of all FileIds in DataSet".
    bool is_everything() const { return has_everything; }

    const std::vector<FileId> &vector() const { return results; }

    const QueryStatistics stats() const { return stats_; }

    // For when you really need to clone the query result
    QueryResult clone() const { return *this; }
};
