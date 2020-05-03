#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "QString.h"
#include "QueryGraph.h"

enum QueryType { PRIMITIVE = 1, AND = 2, OR = 3, MIN_OF = 4 };

class Query {
   public:
    explicit Query(QString &&qstr);
    explicit Query(uint32_t count, std::vector<Query> &&queries);
    explicit Query(const QueryType &type, std::vector<Query> &&queries);
    Query(const Query &other) = delete;
    Query(Query &&other) = default;

    const std::vector<Query> &as_queries() const;
    const QString &as_value() const;
    uint32_t as_count() const;
    std::string as_string_repr() const;
    const QueryType &get_type() const;
    bool operator==(const Query &other) const;

    // Converts this instance of Query to equivalent QueryGraph.
    QueryGraph to_graph(IndexType ntype) const;

   private:
    QueryType type;
    uint32_t count;              // used for QueryType::MIN_OF
    QString value;               // used for QueryType::PRIMITIVE
    std::vector<Query> queries;  // used for QueryType::AND/OR
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
