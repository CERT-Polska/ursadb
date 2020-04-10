#pragma once

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "Core.h"
#include "Utils.h"

enum QueryType { PRIMITIVE = 1, AND = 2, OR = 3, MIN_OF = 4 };

class QueryResult {
   private:
    std::vector<FileId> results;
    bool has_everything;

    QueryResult() : has_everything(true) {}

   public:
    QueryResult(std::vector<FileId> results)
        : results(results), has_everything(false) {}

    static QueryResult empty() { return QueryResult(std::vector<FileId>()); }

    static QueryResult everything() { return QueryResult(); }

    void do_or(const QueryResult &&other);

    void do_and(const QueryResult &&other);

    // If true, means that QueryResults represents special
    // "uninitialized" value, "set of all FileIds in DataSet".
    bool is_everything() const { return has_everything; }

    const std::vector<FileId> &vector() const { return results; }
};

class Query {
   public:
    explicit Query(const QString &qstr);
    explicit Query(unsigned int count, const std::vector<Query> &queries);
    explicit Query(const QueryType &type, const std::vector<Query> &queries);

    const std::vector<Query> &as_queries() const;
    const QString &as_value() const;
    unsigned int as_count() const;
    std::string as_string_repr() const;
    const QueryType &get_type() const;
    bool operator==(Query other) const;

   private:
    QueryType type;
    unsigned int count;          // used for QueryType::MIN_OF
    QString value;               // used for QueryType::PRIMITIVE
    std::vector<Query> queries;  // used for QueryType::AND/OR
};

Query q(const QString &qstr);
Query q_and(const std::vector<Query> &queries);
Query q_or(const std::vector<Query> &queries);
Query q_min_of(unsigned int count, const std::vector<Query> &queries);
std::ostream &operator<<(std::ostream &os, const Query &query);
