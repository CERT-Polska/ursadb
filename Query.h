#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Core.h"

enum QueryType {
    PRIMITIVE = 1,
    AND = 2,
    OR = 3
};

class Query {
public:
    explicit Query(const TriGram &trigram);
    explicit Query(const std::string &str);
    explicit Query(const QueryType &type, const std::vector<Query> &queries);

    const std::vector<Query> &as_queries() const;
    const TriGram &as_trigram() const;
    const QueryType &get_type() const;
    void print_query() const;

private:
    QueryType type;
    TriGram trigram;
    std::vector<Query> queries;
};

Query q(const std::string &str);
Query q_and(const std::vector<Query> &queries);
Query q_or(const std::vector<Query> &queries);
