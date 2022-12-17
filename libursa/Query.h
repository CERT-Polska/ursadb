#pragma once

#include <functional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DatabaseConfig.h"
#include "QString.h"
#include "QueryGraph.h"
#include "QueryResult.h"
#include "Utils.h"

enum class QueryType { PRIMITIVE = 1, AND = 2, OR = 3, MIN_OF = 4 };

// Small utility class to represent a ngram along with its type.
// This is different to the TriGram typedef, because TriGram doesn't know what
// type of index it represents.
class PrimitiveQuery {
   public:
    PrimitiveQuery(IndexType itype, TriGram trigram)
        : itype(itype), trigram(trigram) {}

    const IndexType itype;
    const TriGram trigram;
};

using QueryPrimitive =
    std::function<QueryResult(PrimitiveQuery, QueryCounters *counter)>;

// Query represents the query as provided by the user.
// Query can contain subqueries (using AND/OR/MINOF) or be a literal query.
// There are actually two types of literal query objects - "plain" and
// "planned". All queries start as plain - represented by QString. They are
// independent of the database. Before actually running them, they must be
// planned (using a plan() method). At this point query decides which ngrams
// will actually be checked.
class Query {
   private:
    Query(const Query &other) : type(other.type), ngram(), count(other.count) {
        queries.reserve(other.queries.size());
        for (const auto &query : other.queries) {
            queries.emplace_back(query.clone());
        }
        value.reserve(other.value.size());
        for (const auto &token : other.value) {
            value.emplace_back(token.clone());
        }
    }

    explicit Query(PrimitiveQuery ngram)
        : type(QueryType::PRIMITIVE), ngram(ngram), value() {}

   public:
    explicit Query(QString &&qstr);
    explicit Query(uint32_t count, std::vector<Query> &&queries);
    explicit Query(const QueryType &type, std::vector<Query> &&queries);
    Query(Query &&other) = default;

    const std::vector<Query> &as_queries() const;
    const QString &as_value() const;
    uint32_t as_count() const;
    std::string as_string_repr() const;
    const QueryType &get_type() const;
    bool operator==(const Query &other) const;

    QueryResult run(const QueryPrimitive &primitive,
                    QueryCounters *counters) const;
    Query plan(const std::unordered_set<IndexType> &types_to_query) const;

    Query clone() const { return Query(*this); }

   private:
    QueryType type;
    // used for QueryType::PRIMITIVE
    QString value;                        // before plan()
    std::optional<PrimitiveQuery> ngram;  // after plan()
    // used for QueryType::MIN_OF
    uint32_t count;
    // used for QueryType::AND/OR/MIN_OF
    std::vector<Query> queries;
};

// Creates a literal query. Literals can contain wildcards and alternatives.
Query q(QString &&qstr);

// Creates a query of type "and".
Query q_and(std::vector<Query> &&queries);

// Creates a query of type "or".
Query q_or(std::vector<Query> &&queries);

// Creates a query of type "min of" (query that accepts files that occur in
// at least N subqueries).
Query q_min_of(uint32_t count, std::vector<Query> &&queries);

// Pretty-print the current query instance.
std::ostream &operator<<(std::ostream &os, const Query &query);

// Expands the query to a query graph, but at the same time is careful not to
// generate a query graph that is too big.
// Token validator checks if the specified character can occur at the specified
// position in the stream (otherwise ngram won't be generated).
QueryGraph to_query_graph(const QString &str, int size,
                          const DatabaseConfig &config,
                          const TokenValidator &is_ok);
