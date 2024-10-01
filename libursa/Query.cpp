#include "Query.h"

#include "Utils.h"
#include "spdlog/spdlog.h"

bool PrimitiveQuery::operator<(const PrimitiveQuery &rhs) const {
    if (itype < rhs.itype) {
        return true;
    };
    if (itype > rhs.itype) {
        return false;
    };
    return trigram < rhs.trigram;
}

const std::vector<Query> &Query::as_queries() const {
    if (type != QueryType::AND && type != QueryType::OR &&
        type != QueryType::MIN_OF) {
        throw std::runtime_error("This query doesn\'t contain subqueries.");
    }
    return queries;
}

std::vector<Query> &Query::as_queries() {
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
            os << "and(";
        } else if (type == QueryType::OR) {
            os << "or(";
        } else if (type == QueryType::MIN_OF) {
            os << "min_of(" << query.as_count() << ", ";
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
        os << query.as_string_repr();
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

PrimitiveQuery Query::as_ngram() const {
    if (type != QueryType::PRIMITIVE) {
        throw std::runtime_error("This query doesn\'t contain a ngram.");
    }
    return ngram;
}

std::string Query::as_string_repr() const {
    std::string out = "";
    if (value.empty()) {
        // Query is already after planning stage. Show low-level representation.
        // First digit is the index type and the rest is the raw trigram.
        // Underscore was picked to make copying from terminal easier.
        return fmt::format("{}_{:06x}", (int)ngram.itype, ngram.trigram);
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

// For primitive queries, find a minimal covering set of ngram queries and
// return it. If there are multiple disconnected components, AND them.
// For example, "abcde\x??efg" will return abcd & bcde & efg
Query plan_qstring(const std::unordered_set<IndexType> &types_to_query,
                   const QString &value) {
    std::vector<Query> plan;

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
                plan.emplace_back(PrimitiveQuery(IndexType::WIDE8, *gram));
                skip_to = i + 6;
                i += 2;
                continue;
            }
        }
        // If text4 index is supported, try to add a token and skip 2 bytes.
        if (has_text4) {
            if (const auto &gram = convert_gram(IndexType::TEXT4, i, value)) {
                plan.emplace_back(PrimitiveQuery(IndexType::TEXT4, *gram));
                skip_to = i + 2;
                i += 1;
                continue;
            }
        }
        // If hash4 index is supported and current ngram is not text, try hash4.
        const auto &hgram = convert_gram(IndexType::HASH4, i, value);
        if (i >= (skip_to - 1) && has_hash4 && hgram) {
            plan.emplace_back(PrimitiveQuery(IndexType::HASH4, *hgram));
            // Don't continue here - gram3 can give us more information.
        }
        // Otherwise, add a regular gram3 token.
        const auto &gram = convert_gram(IndexType::GRAM3, i, value);
        if (i >= skip_to && gram) {
            if (has_gram3) {
                plan.emplace_back(PrimitiveQuery(IndexType::GRAM3, *gram));
            }
            i += 1;
            continue;
        }
        // If no ngram can be added, remember to move forward.
        i += 1;
    }

    return q_and(std::move(plan));
}

// Query provided by user contains generic strings like "asdf".
// To actually do the query, we need to convert generic strings to n-grams.
// This is done using plan_qstring, everything else is copied unchanged.
Query Query::plan(const std::unordered_set<IndexType> &types_to_query) const {
    if (type != QueryType::PRIMITIVE) {
        std::vector<Query> plans;
        for (const auto &query : queries) {
            plans.emplace_back(query.plan(types_to_query));
        }
        if (type == QueryType::MIN_OF) {
            return Query(count, std::move(plans));
        }
        return Query(type, std::move(plans));
    }

    return plan_qstring(types_to_query, value);
}

// Prefetch the next `howmany` ngrams.
// This doesn't recurse into other queries. It's not a big problem,
// because all primitives that we can fetch are in long AND sequences.
// But in the future we may consider improving this.
void Query::prefetch(int from_index, int howmany, bool only_last,
                     const PrefetchFunc &prefetcher) const {
    for (int i = 0; i < howmany; i++) {
        int ndx = i + from_index;
        if (ndx >= queries.size()) {
            break;
        }
        if (queries[ndx].type == QueryType::PRIMITIVE) {
            if (only_last && (i + 1 != howmany)) {
                continue;
            }
            spdlog::debug("prefetching {}", ndx);
            prefetcher(queries[ndx].ngram);
        }
    }
}

QueryResult Query::run(const QueryPrimitive &primitive,
                       const PrefetchFunc &prefetcher,
                       QueryCounters *counters) const {
    // Case: primitive query - reduces to AND with tokens from query plan.
    if (type == QueryType::PRIMITIVE) {
        return primitive(ngram, counters);
    }

    constexpr int PRETECTH_RANGE = 3;
    prefetch(0, PRETECTH_RANGE, false, prefetcher);

    // Case: and. Short circuits when result is already empty.
    if (type == QueryType::AND) {
        auto result = QueryResult::everything();
        for (int i = 0; i < queries.size(); i++) {
            prefetch(i + 1, PRETECTH_RANGE, true, prefetcher);
            const auto &query = queries[i];
            result.do_and(query.run(primitive, prefetcher, counters),
                          &counters->ands());
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
            result.do_or(query.run(primitive, prefetcher, counters),
                         &counters->ors());
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
            QueryResult next = query.run(primitive, prefetcher, counters);
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
