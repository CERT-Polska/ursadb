#include "QueryOptimizer.h"

#include <vector>

// Returns a query that represents every element in the dataset.
// There's no magic here, AND() just behaves like this.
Query q_everything() {
    std::vector<Query> queries;
    return std::move(q_and(std::move(queries)));
}

// Checks if the query represents every value in the dataset.
// The only cases when it happens are empty and() and min 0 of (...).
// "min 0 of (...)" is simplified in another pass, so just check for and().
// See also comment above.
bool is_everything(const Query &q) {
    return q.get_type() == QueryType::AND && q.as_queries().size() == 0;
}

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

// This optimization simplifies trivial (one operand) operations:
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

// This optimization gets rid of duplicated primitive queries.
// AND(a, a, a, a, b, b) == AND(a, b)
// This also applies to OR(), but it'll happen very rarely.
Query deduplicate_primitives(Query &&q, bool *changed) {
    if (q.get_type() != QueryType::AND && q.get_type() != QueryType::OR) {
        return std::move(q);
    }

    std::set<PrimitiveQuery> seen;
    std::vector<Query> newqueries;
    for (auto &&query : q.as_queries()) {
        if (query.get_type() != QueryType::PRIMITIVE) {
            newqueries.emplace_back(std::move(query));
        } else if (seen.count(query.as_ngram()) == 0) {
            newqueries.emplace_back(std::move(query));
            seen.insert(query.as_ngram());
        } else {
            *changed = true;
        }
    }
    return std::move(Query(q.get_type(), std::move(newqueries)));
}

// Minof is the slowest operation, so replace it by others if possible.
// This may also enable other optimizations to take place.
// MIN 5 OF (a, b, c, d, e) --> AND(a, b, c, d, e)
// MIN 1 OF (a, b, c, d, e) --> OR(a, b, c, d, e)
// MIN 0 OF (a, b, c, d, e) --> everything()
Query simplify_minof(Query &&q, bool *changed) {
    if (q.get_type() == QueryType::MIN_OF) {
        if (q.as_count() == q.as_queries().size()) {
            *changed = true;
            return std::move(q_and(std::move(q.as_queries())));
        } else if (q.as_count() == 1) {
            *changed = true;
            return std::move(q_or(std::move(q.as_queries())));
        } else if (q.as_count() == 0) {
            *changed = true;
            return std::move(q_everything());
        }
    }
    return std::move(q);
}

// Propagate 'everything' results through the query. For example,
// if we know that C is everything, we immediately know OR(a, b, C)
// OR(AND(), b, c) --> AND()
// MIN 3 OF (AND(), b, c, d) --> MIN 2 OF (b, c, d)
Query propagate_degenerate_queries(Query &&q, bool *changed) {
    if (q.get_type() == QueryType::MIN_OF) {
        std::vector<Query> newqueries;
        uint32_t count = q.as_count();
        for (auto &&query : q.as_queries()) {
            if (is_everything(query)) {
                count -= 1;
                *changed = true;
                if (count == 0) {
                    return std::move(q_everything());
                }
            } else {
                newqueries.emplace_back(std::move(query));
            }
        }
        return std::move(q_min_of(count, std::move(newqueries)));
    }
    if (q.get_type() == QueryType::OR) {
        for (auto &&query : q.as_queries()) {
            if (is_everything(query)) {
                *changed = true;
                return std::move(q_everything());
            }
        }
    }
    return std::move(q);
}

// This heuristic should ideally measure "what is the chance
// that this query returns zero results", or "how many files we expect to get".
// Of course, less files and bigger chance for zero result is better.
// This should also be weighted by the query cost (100 queries for 10% chance
// to get empty result is worse than 2 queries for 15% chance of empty result).
//
// The current implementation is a very naive heuristic, that just looks at
// the query type, and index type for primitives, and orders basing on that.
uint32_t query_heuristic_cost(const Query &q) {
    // From empirical test, order of query types doesn't seem to matter much.
    switch (q.get_type()) {
        case QueryType::PRIMITIVE:
            // Sort by ngram type, then by ngram value, alphabetically first.
            // This is (un)surprisingly important for two reasons:
            // 1. we read sequentially as many ngrams as possible.
            // 2. consecutive ngrams are independent: (abc, bcd) vs (abc, def).
            // Use smaller indexes first, because they're faster to read.
            switch (q.as_ngram().itype) {
                case IndexType::WIDE8:
                    return (0 << 24) + q.as_ngram().trigram;
                case IndexType::TEXT4:
                    return (1 << 24) + q.as_ngram().trigram;
                case IndexType::HASH4:
                    return (2 << 24) + q.as_ngram().trigram;
                case IndexType::GRAM3:
                    return (3 << 24) + q.as_ngram().trigram;
            }
        case QueryType::AND:
            return 4 << 24;
        case QueryType::MIN_OF:
            return 5 << 24;
        case QueryType::OR:
            // OR is the worst operation, since it always needs to scan
            // all of its arguments (no chance of early exit).
            return 6 << 24;
    }
    throw std::runtime_error("Unexpected query/index type.");
}

// Order queries by their heuristic cost.
bool query_heuristic_comparer(const Query &left, const Query &right) {
    return query_heuristic_cost(left) < query_heuristic_cost(right);
}

// Order the subqueries to maximize the chance of early exit.
// This is done after all other optimizations, and there's no point of
// running this in a loop.
Query reorder_subqueries(Query &&q) {
    if (q.get_type() == QueryType::AND) {
        std::stable_sort(q.as_queries().begin(), q.as_queries().end(),
                         query_heuristic_comparer);
    }
    return std::move(q);  // Currently only support AND operators.
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
        q = deduplicate_primitives(std::move(q), &changed);
        q = simplify_minof(std::move(q), &changed);
        q = propagate_degenerate_queries(std::move(q), &changed);
    }
    q = reorder_subqueries(std::move(q));
    return std::move(q);
}
