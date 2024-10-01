#include "QueryOptimizer.h"

#include <vector>

Query q_optimize(Query &&q) {
    if (q.get_type() == QueryType::PRIMITIVE) {
        // Nothing to improve here.
        return std::move(q);
    }

    // Optimization passes will be added here later.

    return std::move(q);
}
