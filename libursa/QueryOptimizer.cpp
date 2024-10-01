#include "QueryOptimizer.h"

#include <vector>

// Run the optimization pases on subqueries.
// After this step, every subquery should be maximally optimized,
// So I believe there's no need to run this in a loop.
Query simplify_subqueries(Query &&q) {
    // q_optimize ensures QueryType is not PRIMITIVE already
    std::vector<Query> newqueries;
    for (auto &&query : q.as_queries()) {
        newqueries.emplace_back(q_optimize(std::move(query)));
    }
    if (q.get_type() == QueryType::MIN_OF) {
        return q_min_of(q.as_count(), std::move(newqueries));
    }
    return std::move(Query(q.get_type(), std::move(newqueries)));
}

Query q_optimize(Query &&q) {
    if (q.get_type() == QueryType::PRIMITIVE) {
        // Nothing to improve here.
        return std::move(q);
    }

    q = simplify_subqueries(std::move(q));

    // Optimization passes will be added here later.

    return std::move(q);
}
