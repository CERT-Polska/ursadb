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

// This optimization simplifies trivial (one operant) operations:
// AND(x) --> x
// OR(x)  --> x
Query flatten_trivial_operations(Query &&q, bool *changed) {
    if (q.get_type() == QueryType::AND && q.as_queries().size() == 1) {
        *changed = true;
        return std::move(q.as_queries()[0]);
    }
    if (q.get_type() == QueryType::OR && q.as_queries().size() == 1) {
        *changed = true;
        return std::move(q.as_queries()[0]);
    }
    return std::move(q);
}

// This optimization inlines eliglible suboperations:
// AND(AND(a, b), AND(c, d), e) --> AND(a, b, c, d, e)
// OR(OR(a, b), OR(c, d), e) --> OR(a, b, c, d, e)
Query inline_suboperations(Query &&q, bool *changed) {
    if (q.get_type() != QueryType::AND && q.get_type() != QueryType::OR) {
        return std::move(q);
    }
    std::vector<Query> newqueries;
    for (auto &&query : q.as_queries()) {
        if (query.get_type() == q.get_type()) {
            for (auto &&subquery : query.as_queries()) {
                newqueries.emplace_back(std::move(subquery));
            }
            *changed = true;
        } else {
            newqueries.emplace_back(std::move(query));
        }
    }
    return std::move(Query(q.get_type(), std::move(newqueries)));
}

Query q_optimize(Query &&q) {
    if (q.get_type() == QueryType::PRIMITIVE) {
        // Nothing to improve here.
        return std::move(q);
    }

    q = simplify_subqueries(std::move(q));
    bool changed = true;
    while (changed) {
        changed = false;
        q = flatten_trivial_operations(std::move(q), &changed);
        q = inline_suboperations(std::move(q), &changed);
    }

    return std::move(q);
}
