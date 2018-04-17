#pragma once

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "Core.h"
#include "Utils.h"

enum QueryType { PRIMITIVE = 1, AND = 2, OR = 3 };

class QueryResult {
  private:
    std::vector<FileId> results;
    bool has_everything;

    QueryResult() : has_everything(true) {}

  public:
    QueryResult(std::vector<FileId> results) : has_everything(false), results(results) {}

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
    explicit Query(const std::string &str);
    explicit Query(const QueryType &type, const std::vector<Query> &queries);

    const std::vector<Query> &as_queries() const;
    const std::string &as_value() const;
    const QueryType &get_type() const;

  private:
    QueryType type;
    std::string value;
    std::vector<Query> queries;
};

Query q(const std::string &str);
Query q_and(const std::vector<Query> &queries);
Query q_or(const std::vector<Query> &queries);
std::ostream &operator<<(std::ostream &os, const Query &query);
