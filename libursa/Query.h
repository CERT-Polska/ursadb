#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "Core.h"

enum QueryType { PRIMITIVE = 1, AND = 2, OR = 3, MIN_OF = 4 };

class Query {
   public:
    explicit Query(const QString &qstr);
    explicit Query(unsigned int count, std::vector<Query> &&queries);
    explicit Query(const QueryType &type, std::vector<Query> &&queries);
    Query(const Query &other) = delete;
    Query(Query &&other) = default;

    const std::vector<Query> &as_queries() const;
    const QString &as_value() const;
    unsigned int as_count() const;
    std::string as_string_repr() const;
    const QueryType &get_type() const;
    bool operator==(const Query &other) const;

   private:
    QueryType type;
    unsigned int count;          // used for QueryType::MIN_OF
    QString value;               // used for QueryType::PRIMITIVE
    std::vector<Query> queries;  // used for QueryType::AND/OR
};

Query q(const QString &qstr);
Query q_and(std::vector<Query> &&queries);
Query q_or(std::vector<Query> &&queries);
Query q_min_of(unsigned int count, std::vector<Query> &&queries);
std::ostream &operator<<(std::ostream &os, const Query &query);
