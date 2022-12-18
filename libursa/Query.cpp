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

uint32_t Query::as_count() const { return count; }

Query::Query(const QueryType &type, std::vector<Query> &&queries)
    : type(type), count(0), queries(std::move(queries)) {}

Query::Query(uint32_t count, std::vector<Query> &&queries)
    : type(QueryType::MIN_OF), count(count), queries(std::move(queries)) {}

Query::Query(QString &&qstr)
    : type(QueryType::PRIMITIVE), count(0), value(std::move(qstr)) {}

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

std::string Query::as_string_repr() const {
    std::string out = "";
    if (ngram != std::nullopt) {
        // Query is already after planning stage. Show low-level representation.
        return fmt::format("{:x}", ngram->trigram);
    }
    // No query plan yet. Show stringlike representation.
    for (const auto &token : value) {
        if (token.num_possible_values() == 1) {
            out += token.possible_values()[0];
        } else {
            out += "?";
        }
    }
    return out;
}

Query q(QString &&qstr) { return Query(std::move(qstr)); }

Query q_and(std::vector<Query> &&queries) {
    return Query(QueryType::AND, std::move(queries));
}

Query q_or(std::vector<Query> &&queries) {
    return Query(QueryType::OR, std::move(queries));
}

Query q_min_of(uint32_t count, std::vector<Query> &&queries) {
    return Query(count, std::move(queries));
}

// Returns a subset of a given QToken, with values accepted by TokenValidator.
QToken filter_qtoken(const QToken &token, uint32_t off,
                     const TokenValidator &is_ok) {
    std::vector<uint8_t> possible_values;
    for (uint8_t val : token.possible_values()) {
        if (is_ok(off, val)) {
            possible_values.push_back(val);
        }
    }
    return QToken::with_values(std::move(possible_values));
}

QueryGraph to_query_graph(const QString &str, int size,
                          const DatabaseConfig &config,
                          const TokenValidator &is_ok) {
    // Graph representing the final query equivalent to qstring.
    QueryGraph result;

    // Maximum number of possible values for the edge to be considered.
    // If token has more than MAX_EDGE possible values, it will never start
    // or end a subgraph. This is to avoid starting a subquery with `??`.
    const uint64_t max_edge = config.get(ConfigKey::query_max_edge());

    // Maximum number of possible values for ngram to be considered. If ngram
    // has more than MAX_NGRAM possible values, it won't be included in the
    // graph and the graph will be split into one or more subgraphs.
    const uint64_t max_ngram = config.get(ConfigKey::query_max_ngram());

    spdlog::debug("Expand+prune for a query graph size={}", size);

    // Offset from the beginning of the string.
    size_t offset = 0;

    // Offset from the beginning of the next connected string component.
    size_t frag_offset = 0;
    while (offset < str.size()) {
        spdlog::debug("Looking for a new start edge {}", offset);
        frag_offset = 0;

        // Check if this node can become an edge.
        if (str[offset].num_possible_values() > max_edge) {
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
        if (static_cast<int32_t>(tokens.size()) < size - 1) {
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
            if (num_possible > max_ngram) {
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

        // Finally, prune the subquery from the right (using max_edge).
        while (tokens.back().empty() ||
               tokens.back().num_possible_values() > max_edge) {
            // This is safe, because there must be at least one element.
            tokens.pop_back();
            if (tokens.empty()) {
                break;
            }
        }

        // If there are not enough good tokens, go forward.
        if (static_cast<int>(tokens.size()) < size) {
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

// For primitive queries, find a minimal covering set of ngram queries and
// return it. If there are multiple disconnected components, AND them.
// For example, "abcde\x??efg" will return abcd & bcde & efg
std::vector<PrimitiveQuery> plan_qstring(
    const std::unordered_set<IndexType> &types_to_query, const QString &value) {
    std::vector<PrimitiveQuery> plan;

    bool has_gram3 = types_to_query.count(IndexType::GRAM3) != 0;
    bool has_text4 = types_to_query.count(IndexType::TEXT4) != 0;
    bool has_wide8 = types_to_query.count(IndexType::WIDE8) != 0;
    bool has_hash4 = types_to_query.count(IndexType::HASH4) != 0;

    // `i` is the current index. `skip_to` is used to keep track of last
    // "handled" byte. For example there's no point in adding 3gram "bcd"
    // when 4gram "abcd" was already added. Only relevant for wide8 ngrams.
    int i = 0;
    int skip_to = 0;
    while (i + 2 < value.size()) {
        // If wide8 index is supported, try to add a token and skip 6 bytes.
        if (has_wide8) {
            if (const auto &gram = convert_gram(IndexType::WIDE8, i, value)) {
                plan.emplace_back(IndexType::WIDE8, *gram);
                skip_to = i + 6;
                i += 2;
                continue;
            }
        }
        // If text4 index is supported, try to add a token and skip 2 bytes.
        if (has_text4) {
            if (const auto &gram = convert_gram(IndexType::TEXT4, i, value)) {
                plan.emplace_back(IndexType::TEXT4, *gram);
                skip_to = i + 2;
                i += 1;
                continue;
            }
        }
        // If hash4 index is supported and current ngram is not text, try hash4.
        const auto &hgram = convert_gram(IndexType::HASH4, i, value);
        if (i >= (skip_to - 1) && has_hash4 && hgram) {
            plan.emplace_back(IndexType::HASH4, *hgram);
            // Don't continue here - gram3 can give us more information.
        }
        // Otherwise, add a regular gram3 token.
        const auto &gram = convert_gram(IndexType::GRAM3, i, value);
        if (i >= skip_to && gram) {
            if (has_gram3) {
                plan.emplace_back(IndexType::GRAM3, *gram);
            }
            i += 1;
            continue;
        }
        // If no ngram can be added, remember to move forward.
        i += 1;
    }

    return std::move(plan);
}

uint64_t Query::rarity(const PrimitiveEvaluator &evaluator) const {
    if (type == QueryType::PRIMITIVE) {
        return evaluator(*ngram);
    } else {
        return 0x100000000;
    }
}

Query Query::plan(const std::unordered_set<IndexType> &types_to_query, const PrimitiveEvaluator &evaluate) const {
    if (type == QueryType::PRIMITIVE) {
        if (ngram != std::nullopt) {
            // Query already as simple as possible
            return Query(*ngram);
        }

        std::vector<Query> plans;
        for (const auto gram : plan_qstring(types_to_query, value)) {
            plans.emplace_back(Query(gram));
        }
        return Query(QueryType::AND, std::move(plans)).plan(types_to_query, evaluate);
    }

    std::vector<Query> plans;
    for (const auto &query : queries) {
        plans.emplace_back(query.plan(types_to_query, evaluate));
    }

    // Special case `1 of ...` (OR) and `n of (1, 2, ... n)` (AND).
    if (type == QueryType::MIN_OF) {
        if (count == 1) {
            return Query(QueryType::OR, std::move(plans)).plan(types_to_query, evaluate);
        }
        if (count == plans.size()) {
            return Query(QueryType::AND, std::move(plans)).plan(types_to_query, evaluate);
        }
        return Query(count, std::move(plans));
    }

    // For all other types (AND and OR), flatten and simplify recursively
    std::vector<Query> new_plans;
    for (auto it = plans.begin(); it != plans.end(); it++) {
        if (it->type == type) {
            for (auto &subplan : it->queries) {
                new_plans.emplace_back(std::move(subplan));
            }
        } else {
            new_plans.emplace_back(std::move(*it));
        }
    }
    std::sort(new_plans.begin(), new_plans.end(), [&evaluate](const auto &l, const auto &r) { return l.rarity(evaluate) < r.rarity(evaluate); });
    return Query(type, std::move(new_plans));
}

QueryResult Query::run(const QueryPrimitive &primitive,
                       QueryCounters *counters) const {
    // Case: primitive query - reduces to AND with tokens from query plan.
    if (type == QueryType::PRIMITIVE) {
        auto result = QueryResult::everything();
        return primitive(*ngram, counters);
    }
    // Case: and. Short circuits when result is already empty.
    if (type == QueryType::AND) {
        auto result = QueryResult::everything();
        for (const auto &query : queries) {
            result.do_and(query.run(primitive, counters), &counters->ands());
            if (result.is_empty()) {
                break;
            }
        }
        return result;
    }
    // Case: or. Short circuits when result is already everything.
    if (type == QueryType::OR) {
        auto result = QueryResult::empty();
        for (const auto &query : queries) {
            result.do_or(query.run(primitive, counters), &counters->ors());
            if (result.is_everything()) {
                break;
            }
        }
        return result;
    }
    // Case: minof. We remove `everything` and `empty` sets from inputs, and
    // adjust cutoff or input size appropriately. Short circuits in two cases:
    // - cutoff is <= 0 (for example: 0 of ("a", "b")).
    // - cutoff is > size(inputs) (for example: 3 of ("a", "b"))
    // There is some logic duplication here and in QueryResult::do_min_of_real.
    if (type == QueryType::MIN_OF) {
        std::vector<QueryResult> results;
        std::vector<const QueryResult *> results_ptrs;
        results.reserve(queries.size());
        results_ptrs.reserve(queries.size());
        int cutoff = count;
        int nonempty_sources = queries.size();
        for (const auto &query : queries) {
            QueryResult next = query.run(primitive, counters);
            if (next.is_everything()) {
                cutoff -= 1;
                if (cutoff <= 0) {
                    return QueryResult::everything();
                }
            } else if (next.is_empty()) {
                nonempty_sources -= 1;
                if (cutoff > nonempty_sources) {
                    return QueryResult::empty();
                }
            } else {
                results.emplace_back(std::move(next));
                results_ptrs.emplace_back(&results.back());
            }
        }
        return QueryResult::do_min_of(cutoff, results_ptrs,
                                      &counters->minofs());
    }
    throw std::runtime_error("Unexpected query type");
}
