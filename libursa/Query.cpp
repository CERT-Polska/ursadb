#include "Query.h"

const std::vector<Query> &Query::as_queries() const {
    if (type != QueryType::AND && type != QueryType::OR &&
        type != QueryType::MIN_OF) {
        throw std::runtime_error("This query doesn\'t contain subqueries.");
    }

    return queries;
}

unsigned int Query::as_count() const { return count; }

Query::Query(const QueryType &type, std::vector<Query> &&queries)
    : type(type), count(0), queries(std::move(queries)) {}

Query::Query(unsigned int count, std::vector<Query> &&queries)
    : type(QueryType::MIN_OF), count(count), queries(std::move(queries)) {}

Query::Query(QString &&qstr)
    : type(QueryType::PRIMITIVE), count(0), value(std::move(qstr)), queries() {}

bool Query::operator==(const Query &other) const {
    return type == other.type && value == other.value &&
           queries == other.queries;
}

std::ostream &operator<<(std::ostream &os, const Query &query) {
    QueryType type = query.get_type();
    if (type == QueryType::AND || type == QueryType::OR ||
        type == QueryType::MIN_OF) {
        if (type == QueryType::AND) {
            os << "AND(";
        } else if (type == QueryType::OR) {
            os << "OR(";
        } else if (type == QueryType::MIN_OF) {
            os << "MIN_OF(" << query.as_count() << ", ";
        }

        bool is_first = true;
        for (const auto &q : query.as_queries()) {
            if (!is_first) {
                os << ", ";
            }
            is_first = false;
            os << q;
        }
        os << ")";
    } else if (type == QueryType::PRIMITIVE) {
        os << "'" << query.as_string_repr() << "'";
    } else {
        throw std::runtime_error("Unknown query type.");
    }
    return os;
}

const QueryType &Query::get_type() const { return type; }

const QString &Query::as_value() const {
    if (type != QueryType::PRIMITIVE) {
        throw std::runtime_error("This query doesn\'t have any value.");
    }

    return value;
}

std::string Query::as_string_repr() const { return "[primitive]"; }

Query q(QString &&qstr) { return Query(std::move(qstr)); }

Query q_and(std::vector<Query> &&queries) {
    return Query(QueryType::AND, std::move(queries));
}

Query q_or(std::vector<Query> &&queries) {
    return Query(QueryType::OR, std::move(queries));
}

Query q_min_of(unsigned int count, std::vector<Query> &&queries) {
    return Query(count, std::move(queries));
}
