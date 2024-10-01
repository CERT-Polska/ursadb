#pragma once

#include "Query.h"

// Optimizes a query, and returns the optimized version.
// Optimizations try to simplify the expression in various ways to make the
// execution faster - for example by enabling short-circuiting in some places.
Query q_optimize(Query &&query);
