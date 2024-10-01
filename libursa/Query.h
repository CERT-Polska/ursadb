#pragma once

#include <functional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DatabaseConfig.h"
#include "QString.h"
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

    IndexType itype;
    TriGram trigram;

    // We want to use PrimitiveQuery in STL containers, and this means they
    // must be comparable using <. Specific order doesn't matter.
    bool operator<(const PrimitiveQuery &rhs) const;
};

using QueryPrimitive =
    std::function<QueryResult(PrimitiveQuery, QueryCounters *counter)>;

using PrefetchFunc = std::function<void(PrimitiveQuery)>;

// Query represents the query as provided by the user.
// Query can contain subqueries (using AND/OR/MINOF) or be a literal query.
// There are actually two types of literal query objects - "plain" and
// "planned". All queries start as plain - represented by QString. They are
// independent of the database. Before actually running them, they must be
// planned (using a plan() method). At this point query decides which ngrams
// will actually be checked.
class Query {
   public:
    explicit Query(PrimitiveQuery ngram)
        : type(QueryType::PRIMITIVE), ngram(ngram), value() {}
    explicit Query(QString &&qstr);
    explicit Query(uint32_t count, std::vector<Query> &&queries);
    explicit Query(const QueryType &type, std::vector<Query> &&queries);
    Query(Query &&other) = default;
    Query &operator=(Query &&) = default;

    const std::vector<Query> &as_queries() const;
    std::vector<Query> &as_queries();
    const QString &as_value() const;
    uint32_t as_count() const;
    PrimitiveQuery as_ngram() const;
    std::string as_string_repr() const;
    const QueryType &get_type() const;
    bool operator==(const Query &other) const;

    QueryResult run(const QueryPrimitive &primitive,
                    const PrefetchFunc &prefetch,
                    QueryCounters *counters) const;
    Query plan(const std::unordered_set<IndexType> &types_to_query) const;

   private:
    void prefetch(int from_index, int howmany, bool only_last,
                  const PrefetchFunc &prefetch) const;

    QueryType type;
    // used for QueryType::PRIMITIVE before plan()
    QString value;
    // used for QueryType::PRIMITIVE after plan(). Initial value arbitrary.
    PrimitiveQuery ngram = PrimitiveQuery(IndexType::GRAM3, 0);
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
