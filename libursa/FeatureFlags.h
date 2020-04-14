#pragma once

namespace feature {

#ifdef EXPERIMENTAL_QUERY_GRAPHS
constexpr bool query_graphs = true;
#else
constexpr bool query_graphs = false;
#endif

}  // namespace feature
