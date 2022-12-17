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

    // Counter for unique file reads.
    QueryCounter uniq_reads_;

    // Counter for min ... of operations.
    QueryCounter minofs_;

   public:
    QueryCounters() : reads_{} {}

    QueryCounter &ands() { return ands_; }
    QueryCounter &ors() { return ors_; }
    QueryCounter &reads() { return reads_; }
    QueryCounter &uniq_reads() { return uniq_reads_; }
    QueryCounter &minofs() { return minofs_; }

    void add(const QueryCounters &other);

    // Create a map with all counters (for serialisation).
    std::unordered_map<std::string, QueryCounter> counters() const;
};
