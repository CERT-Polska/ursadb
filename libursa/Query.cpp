#include "Query.h"

#include "Utils.h"
#include "spdlog/spdlog.h"

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

// Returns a subset of a given QToken, with values accepted by TokenValidator.
QToken filter_qtoken(const QToken &token, uint32_t off, TokenValidator is_ok) {
    std::vector<uint8_t> possible_values;
    for (uint8_t val : token.possible_values()) {
        if (is_ok(off, val)) {
            possible_values.push_back(val);
        }
    }
    return QToken::with_values(std::move(possible_values));
}

// Expands the query to a query graph, but at the same time is careful not to
// generate a query graph that is too big.
// Token validator checks if the specified character can occur at the specified
// position in the stream (otherwise ngram won't be generated).
QueryGraph to_query_graph(const QString &str, int size, TokenValidator is_ok) {
    // Graph representing the final query equivalent to qstring.
    QueryGraph result;

    // Maximum number of possible values for the edge to be considered.
    // If token has more than MAX_EDGE possible values, it will never start
    // or end a subgraph. This is to avoid starting a subquery with `??`.
    constexpr uint32_t MAX_EDGE = 16;

    // Maximum number of possible values for ngram to be considered. If ngram
    // has more than MAX_NGRAM possible values, it won't be included in the
    // graph and the graph will be split into one or more subgraphs.
    constexpr uint32_t MAX_NGRAM = 256;

    spdlog::debug("Expand+prune for a query graph size={}", size);

    // Offset from the beginning of the string.
    int offset = 0;

    // Offset from the beginning of the next connected string component.
    int frag_offset = 0;
    while (offset < str.size()) {
        spdlog::debug("Looking for a new start edge {}", offset);
        frag_offset = 0;

        // Check if this node can become an edge.
        if (str[offset].num_possible_values() > MAX_EDGE) {
            offset++;
            continue;
        }

        // Insert first ngram_size - 1 tokens.
        std::vector<QToken> tokens;
        for (int i = 0; i < size - 1 && offset < str.size(); i++) {
            auto token = filter_qtoken(str[offset], frag_offset, is_ok);
            if (token.empty()) {
                break;
            }
            tokens.emplace_back(std::move(token));
            offset += 1;
            frag_offset += 1;
        }

        // If there are no good tokens this time, go forward.
        if (tokens.size() < size - 1) {
            if (frag_offset == 0) {
                offset++;
            }
            continue;
        }

        // Now insert tokens ending ngram, as long as it is small enough.
        while (!tokens.back().empty() && offset < str.size()) {
            uint64_t num_possible = 1;
            for (int i = 0; i < size; i++) {
                num_possible *= str[offset - i].num_possible_values();
            }
            if (num_possible > MAX_NGRAM) {
                break;
            }
            auto token = filter_qtoken(str[offset], frag_offset, is_ok);
            if (token.empty()) {
                break;
            }
            tokens.emplace_back(std::move(token));
            offset += 1;
            frag_offset += 1;
        }

        // Finally, prune the subquery from the right (using MAX_EDGE).
        while (tokens.back().empty() ||
               tokens.back().num_possible_values() > MAX_EDGE) {
            // This is safe, because there must be at least one element.
            tokens.pop_back();
            if (tokens.empty()) {
                break;
            }
        }

        // If there are not enough good tokens, go forward.
        if (tokens.size() < size) {
            continue;
        }

        spdlog::debug("Got a subgraph candidate, size={}", tokens.size());
        QueryGraph subgraph{QueryGraph::from_qstring(tokens)};

        for (int i = 0; i < size - 1; i++) {
            spdlog::debug("Computing dual graph ({} nodes)", subgraph.size());
            subgraph = std::move(subgraph.dual());
        }

        spdlog::debug("Merging subgraph into the result");
        result.and_(std::move(subgraph));
    }

    spdlog::debug("Query graph expansion succeeded ({} nodes)", result.size());
    return result;
}

QueryGraph Query::to_graph(IndexType ntype) const {
    if (type == QueryType::PRIMITIVE) {
        TokenValidator validator = get_validator_for(ntype);
        size_t input_len = get_ngram_size_for(ntype);
        return to_query_graph(value, input_len, validator);
    }

    if (type == QueryType::AND) {
        QueryGraph result;
        for (const auto &query : queries) {
            result.and_(query.to_graph(ntype));
        }
        return result;
    }

    if (type == QueryType::OR) {
        if (queries.empty()) {
            return QueryGraph();
        }
        QueryGraph result = std::move(queries[0].to_graph(ntype));
        for (int i = 1; i < queries.size(); i++) {
            result.or_(queries[i].to_graph(ntype));
        }
        return result;
    }

    if (type == QueryType::MIN_OF) {
        std::vector<QueryGraph> subgraphs;
        for (const auto &query : queries) {
            subgraphs.emplace_back(query.to_graph(ntype));
        }
        return QueryGraph::min_of(count, std::move(subgraphs));
    }
    throw std::runtime_error("Unknown query type.");
}
