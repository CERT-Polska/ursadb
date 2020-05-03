// Based on examples provided by:
// Copyright (c) 2017-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include "QueryParser.h"

#include <string>
#include <type_traits>

#include "Core.h"
#include "FeatureFlags.h"
#include "Query.h"
#include "Version.h"
#include "pegtl/pegtl.hpp"
#include "pegtl/pegtl/contrib/abnf.hpp"
#include "pegtl/pegtl/contrib/parse_tree.hpp"
#include "pegtl/pegtl/contrib/unescape.hpp"

using namespace tao::TAO_PEGTL_NAMESPACE;  // NOLINT

namespace queryparse {

// clang-format off

// tokens
struct comma_token : one<','> {};
struct select_token : TAO_PEGTL_STRING("select") {};
struct status_token : TAO_PEGTL_STRING("status") {};
struct index_token : TAO_PEGTL_STRING("index") {};
struct reindex_token : TAO_PEGTL_STRING("reindex") {};
struct compact_token : TAO_PEGTL_STRING("compact") {};
struct topology_token : TAO_PEGTL_STRING("topology") {};
struct with_token : TAO_PEGTL_STRING("with") {};
struct gram3_token : TAO_PEGTL_STRING("gram3") {};
struct hash4_token : TAO_PEGTL_STRING("hash4") {};
struct text4_token : TAO_PEGTL_STRING("text4") {};
struct wide8_token : TAO_PEGTL_STRING("wide8") {};
struct all_token : TAO_PEGTL_STRING("all") {};
struct smart_token : TAO_PEGTL_STRING("smart") {};
struct ping_token : TAO_PEGTL_STRING("ping") {};
struct dataset_token : TAO_PEGTL_STRING("dataset") {};
struct taints_token : TAO_PEGTL_STRING("taints") {};
struct datasets_token : TAO_PEGTL_STRING("datasets") {};
struct taint_token : TAO_PEGTL_STRING("taint") {};
struct untaint_token : TAO_PEGTL_STRING("untaint") {};
struct from_token : TAO_PEGTL_STRING("from") {};
struct list_token : TAO_PEGTL_STRING("list") {};
struct min_token : TAO_PEGTL_STRING("min") {};
struct of_token : TAO_PEGTL_STRING("of") {};
struct nocheck_token : TAO_PEGTL_STRING("nocheck") {};
struct into_token : TAO_PEGTL_STRING("into") {};
struct iterator_token : TAO_PEGTL_STRING("iterator") {};
struct pop_token : TAO_PEGTL_STRING("pop") {};
struct config_token : TAO_PEGTL_STRING("config") {};
struct get_token : TAO_PEGTL_STRING("get") {};
struct set_token : TAO_PEGTL_STRING("set") {};
struct drop_token : TAO_PEGTL_STRING("drop") {};

// literals

// example: c
struct hexdigit : abnf::HEXDIG {};

// example: ?
struct wildcard : seq<one<'?'>> {};

// example: 1F
// example: ?3
struct hexbyte : seq<sor<hexdigit, wildcard>, sor<hexdigit, wildcard>> {};

// example: x73
struct escaped_x : seq<one<'x'>, hexbyte> {};

// example: n
struct escaped_char : one<'"', '\\', 'b', 'f', 'n', 'r', 't'> {};

// example: n
// example: x73
struct escaped : sor<escaped_char, escaped_x> {};

// example: n
struct ascii_char : utf8::range<0x20, 0x7e> {};

// example: \x73
// example: \n
// example: a
struct character : if_must_else<one<'\\'>, escaped, ascii_char> {};

// example: this \n is \x?1 a \" text
struct string_content : until<at<one<'"'>>, must<character>> {};

// example: "this \n is \x?1 a \" text"
struct plaintext : seq<one<'"'>, must<string_content>, any> {
    using content = string_content;
};

// example: w"this \n is \x?1 a \" text"
struct wide_plaintext : seq<one<'w'>, plaintext> {};

// example: 123
struct number : plus<abnf::DIGIT> {};

// example: &
struct op_and : pad<one<'&'>, space> {};

// example: |
struct op_or : pad<one<'|'>, space> {};

// example: (
struct open_bracket : seq<one<'('>, star<space>> {};

// example: )
struct close_bracket : seq<star<space>, one<')'>> {};

// example: {
struct open_curly : seq<one<'{'>, star<space>> {};

// example: }
struct close_curly : seq<star<space>, one<'}'>> {};

// example: [
struct open_square : seq<one<'['>, star<space>> {};

// example: ]
struct close_square : seq<star<space>, one<']'>> {};

// example: 1B | 3? | ?? | 45
struct hexalternatives : seq<hexbyte, star<seq<star<space>, one<'|'>, star<space>, hexbyte>>> {};

// example: (1B | 3? | ?? | 45)
struct hexoptions : seq<one<'('>, star<space>, hexalternatives, star<space>, one<')'>> {};

// example: 112F (3? | 45 | 1F) 3D
struct hexbytes : opt<list<sor<hexbyte, hexoptions>, star<space>>> {};

// example: {112F (3? | 45 | 1F) 3D}
struct hexstring : if_must<open_curly, hexbytes, close_curly> {};

// example: w"wide"
// example: "plain"
// example: {112F (3? | 45 | 1F) 3D}
struct string_like : sor<wide_plaintext, plaintext, hexstring> {};

// expressions
struct expression;
struct bracketed : if_must<open_bracket, expression, close_bracket> {};
struct value : sor<string_like, bracketed> {};
struct comma;
struct argument_tuple : seq<open_bracket, seq<expression, star<seq<comma, expression>>>, close_bracket> {};
struct min_of_expr : seq<min_token, plus<space>, number, plus<space>, of_token, star<space>, argument_tuple> {};
struct expression : seq<sor<value, min_of_expr>, star<sor<op_and, op_or>, expression>> {};
struct comma : seq<star<space>, comma_token, star<space>> {};
struct plaintext_list : seq<open_square, opt<list<plaintext, comma>>, close_square> {};

// select command
struct with_taints_token : seq<with_token, plus<space>, taints_token> {};
struct with_taints_construct : seq<plus<space>, with_taints_token, plus<space>, plaintext_list> {};
struct with_datasets_token : seq<with_token, plus<space>, datasets_token> {};
struct with_datasets_construct : seq<plus<space>, with_datasets_token, plus<space>, plaintext_list> {};
struct iterator_magic : seq<iterator_token> {};
struct into_iterator_construct : seq<plus<space>, into_token, plus<space>, iterator_magic> {};
struct select_body : seq<star<space>, expression> {};

// dataset command
struct dataset_drop: seq<drop_token> {};
struct dataset_taint: seq<taint_token, plus<space>, plaintext> {};
struct dataset_untaint: seq<untaint_token, plus<space>, plaintext> {};
struct dataset_operation: sor<dataset_taint, dataset_untaint, dataset_drop> {};

// iterator command
struct iterator_pop: seq<pop_token, plus<space>, number> {};

// index command 
struct index_type : sor<gram3_token, hash4_token, text4_token, wide8_token> {};
struct paths_construct : list<string_like, space> {};
struct index_type_list : seq<open_square, opt<list<index_type, comma>>, close_square> {};
struct index_with_construct : seq<star<space>, with_token, star<space>, index_type_list> {};
struct nocheck_construct : seq<star<space>, nocheck_token> {};
struct from_list_construct : seq<from_token, plus<space>, list_token, plus<space>, string_like> {};

// config command 
struct get_construct : seq<get_token, star<space>, opt<list<plaintext, space>>> {};
struct set_construct : seq<set_token, star<space>, plaintext, star<space>, number> {};

// commands
struct select : seq<select_token, opt<with_taints_construct>, opt<with_datasets_construct>, opt<into_iterator_construct>, select_body> {};
struct dataset : seq<dataset_token, star<space>, plaintext, star<space>, dataset_operation> {};
struct iterator : seq<iterator_token, star<space>, plaintext, star<space>, iterator_pop> {};
struct index : seq<index_token, star<space>, sor<paths_construct, from_list_construct>, opt<index_with_construct>, opt<nocheck_construct>> {};
struct reindex : seq<reindex_token, star<space>, string_like, star<space>, index_with_construct> {};
struct compact : seq<compact_token, star<space>, sor<all_token, smart_token>> {};
struct config : seq<config_token, star<space>, sor<get_construct, set_construct>> {};
struct status : seq<status_token> {};
struct topology : seq<topology_token> {};
struct ping : seq<ping_token> {};

// api
struct command : seq<sor<select, index, iterator, reindex, compact, config, status, topology, ping, dataset>, star<space>, one<';'>> {};
struct grammar : seq<command, star<space>, eof> {};

// store configuration (what to keep and what to drop)
template <typename> struct store : std::false_type {};
template <> struct store<plaintext> : std::true_type {};
template <> struct store<with_taints_token> : std::true_type {};
template <> struct store<with_datasets_token> : std::true_type {};
template <> struct store<into_token> : std::true_type {};
template <> struct store<nocheck_construct> : std::true_type {};
template <> struct store<wide_plaintext> : std::true_type {};
template <> struct store<op_and> : parse_tree::remove_content {};
template <> struct store<op_or> : parse_tree::remove_content {};
template <> struct store<expression> : std::true_type {};
template <> struct store<hexstring> : std::true_type {};
template <> struct store<escaped_char> : std::true_type {};
template <> struct store<hexoptions> : std::true_type {};
template <> struct store<hexbyte> : std::true_type {};
template <> struct store<ascii_char> : std::true_type {};
template <> struct store<number> : std::true_type {};
template <> struct store<select> : std::true_type {};
template <> struct store<dataset> : std::true_type {};
template <> struct store<iterator> : std::true_type {};
template <> struct store<index> : std::true_type {};
template <> struct store<reindex> : std::true_type {};
template <> struct store<compact> : std::true_type {};
template <> struct store<config> : std::true_type {};
template <> struct store<get_token> : std::true_type {};
template <> struct store<set_token> : std::true_type {};
template <> struct store<topology> : std::true_type {};
template <> struct store<status> : std::true_type {};
template <> struct store<ping> : std::true_type {};
template <> struct store<index_type_list> : std::true_type {};
template <> struct store<plaintext_list> : std::true_type {};
template <> struct store<taint_token> : std::true_type {};
template <> struct store<untaint_token> : std::true_type {};
template <> struct store<gram3_token> : std::true_type {};
template <> struct store<hash4_token> : std::true_type {};
template <> struct store<text4_token> : std::true_type {};
template <> struct store<wide8_token> : std::true_type {};
template <> struct store<all_token> : std::true_type {};
template <> struct store<smart_token> : std::true_type {};
template <> struct store<min_of_expr> : std::true_type {};
template <> struct store<paths_construct> : std::true_type {};
template <> struct store<from_list_construct> : std::true_type {};
template <> struct store<select_body> : std::true_type {};
template <> struct store<pop_token> : std::true_type {};
template <> struct store<iterator_magic> : std::true_type {};
template <> struct store<drop_token> : std::true_type {};

// clang-format on

int hex2int(char hexchar) {
    if (hexchar >= '0' && hexchar <= '9') {
        return hexchar - '0';
    }
    if (hexchar >= 'a' && hexchar <= 'f') {
        return 10 + hexchar - 'a';
    }
    if (hexchar >= 'A' && hexchar <= 'F') {
        return 10 + hexchar - 'A';
    }
    throw std::runtime_error("invalid hex char");
}

constexpr char unescape_char(char escaped) {
    switch (escaped) {
        case '"':
            return '\"';
        case '\\':
            return '\\';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        default:
            throw std::runtime_error("unexpected escaped char");
    }
}

char transform_char(const parse_tree::node &n) {
    const std::string &content = n.content();
    if (n.is<hexbyte>()) {
        return (char)((hex2int(content[0]) << 4) + hex2int(content[1]));
    }
    if (n.is<ascii_char>()) {
        return content[0];
    }
    if (n.is<escaped_char>()) {
        return unescape_char(content[0]);
    }
    throw std::runtime_error("unknown character parse");
}

std::vector<IndexType> transform_index_types(const parse_tree::node &n) {
    std::vector<IndexType> result;
    for (auto &child : (*&n).children) {
        auto type = index_type_from_string(child->content());
        if (type == std::nullopt) {
            throw std::runtime_error("index type unsupported by parser");
        }
        result.push_back(type.value());
    }
    return result;
}

// Returns a QToken representing a given hexbyte
QToken transform_hexbyte(const parse_tree::node &atom) {
    const std::string &c = atom.content();

    if (c[0] == '?' && c[1] == '?') {  // \x??
        return QToken::wildcard();
    }
    if (c[0] == '?') {  // \x?A
        return QToken::high_wildcard(hex2int(c[1]));
    }
    if (c[1] == '?') {  // \xA?
        return QToken::low_wildcard(hex2int(c[0]) << 4U);
    }
    return QToken::single(transform_char(atom));
}

QString transform_qstring(const parse_tree::node &n) {
    auto *root = &n;

    if (n.is<wide_plaintext>()) {
        // Unpack plaintext from inside wide_plaintext.
        root = n.children[0].get();
    }

    int wildcard_ticks = 0;

    QString result;
    for (auto &atom : root->children) {
        if (atom->is<hexbyte>()) {
            result.emplace_back(transform_hexbyte(*atom));
        } else if (atom->is<hexoptions>()) {
            std::set<uint8_t> opts;
            for (const auto &child : (*&atom)->children) {
                auto child_token{transform_hexbyte(*child)};
                auto &child_opts{child_token.possible_values()};
                opts.insert(child_opts.begin(), child_opts.end());
            }
            std::vector<uint8_t> opt_vector{opts.begin(), opts.end()};
            result.emplace_back(QToken::with_values(std::move(opt_vector)));
        } else {
            result.emplace_back(
                std::move(QToken::single(transform_char(*atom))));

            if (n.is<wide_plaintext>()) {
                result.emplace_back(std::move(QToken::single('\0')));
            }
        }

        wildcard_ticks--;
    }

    return result;
}

std::string transform_string(const parse_tree::node &n) {
    auto *root = &n;
    std::string result;
    for (auto &atom : root->children) {
        result += transform_char(*atom);
    }

    return result;
}

Query transform(const parse_tree::node &n) {
    if (n.is<plaintext>() || n.is<wide_plaintext>() || n.is<hexstring>()) {
        return Query(transform_qstring(n));
    }
    if (n.is<min_of_expr>()) {
        auto &count = n.children[0];
        unsigned int counti;

        try {
            counti = std::stoi(count->content());
        } catch (std::out_of_range &e) {
            throw std::runtime_error(
                "number N is out of range in 'min N of (...)' expression");
        }

        auto it = n.children.cbegin() + 1;
        std::vector<Query> subq;
        for (; it != n.children.cend(); ++it) {
            subq.emplace_back(transform(**it));
        }

        return Query(counti, std::move(subq));
    }
    if (n.is<expression>()) {
        if (n.children.size() == 1) {
            return transform(*n.children[0]);
        }

        auto &expr = n.children[1];
        if (expr->is<op_or>()) {
            std::vector<Query> opts;
            opts.emplace_back(transform(*n.children[0]));
            opts.emplace_back(transform(*n.children[2]));
            return Query(QueryType::OR, std::move(opts));
        }
        if (expr->is<op_and>()) {
            std::vector<Query> opts;
            opts.emplace_back(transform(*n.children[0]));
            opts.emplace_back(transform(*n.children[2]));
            return Query(QueryType::AND, std::move(opts));
        }
        throw std::runtime_error("encountered unexpected expression");
    }
    throw std::runtime_error("encountered unexpected node");
}

Command transform_command(const parse_tree::node &n) {
    if (n.is<select>()) {
        int iter = 0;
        bool use_iterator = false;
        std::set<std::string> taints;
        std::set<std::string> datasets;
        while (true) {
            if (n.children[iter]->is<with_taints_token>()) {
                // handle `with taints ["xxx", "yyy"]` construct
                for (const auto &taint : n.children[iter + 1]->children) {
                    taints.insert(transform_string(*taint));
                }
                iter += 2;
                continue;
            }
            if (n.children[iter]->is<with_datasets_token>()) {
                // handle `with datasets ["xxx", "yyy"]` construct
                for (const auto &dataset : n.children[iter + 1]->children) {
                    datasets.insert(transform_string(*dataset));
                }
                iter += 2;
                continue;
            }
            if (n.children[iter]->is<into_token>()) {
                // handle `into iterator` construct
                if (!n.children[iter + 1]->is<iterator_magic>()) {
                    throw std::runtime_error("unsupported select mode");
                }
                use_iterator = true;
                iter += 2;
                continue;
            }
            if (n.children[iter]->is<select_body>()) {
                // select_body is always the last part of the query
                break;
            }
            throw std::runtime_error("unexpected node in select");
        }
        const auto &expr = n.children[iter]->children[0];
        return Command(
            SelectCommand(transform(*expr), taints, datasets, use_iterator));
    }
    if (n.is<iterator>()) {
        if (!n.children[1]->is<pop_token>()) {
            throw std::runtime_error("Unknown iterator mode");
        }
        std::string iterator_id = transform_string(*n.children[0]);
        uint64_t pop = std::stoi(n.children[2]->content());
        return Command(IteratorPopCommand(iterator_id, pop));
    }
    if (n.is<index>()) {
        auto &target_n = n.children[0];

        std::vector<std::string> paths;
        std::vector<IndexType> types = default_index_types();
        bool ensure_unique = true;

        for (size_t mod = 1; mod < n.children.size(); mod++) {
            if (n.children[mod]->is<nocheck_construct>()) {
                ensure_unique = false;
            } else if (n.children[mod]->is<index_type_list>()) {
                types = transform_index_types(*n.children[1]);
            }
        }

        if (target_n->is<paths_construct>()) {
            for (const auto &it : target_n->children) {
                paths.push_back(transform_string(*it));
            }

            return Command(IndexCommand(paths, types, ensure_unique));
        }
        if (target_n->is<from_list_construct>()) {
            std::string list_file = transform_string(*target_n->children[0]);
            return Command(IndexFromCommand(list_file, types, ensure_unique));
        }
        throw std::runtime_error("unexpected first node in index");
    }
    if (n.is<reindex>()) {
        std::string dataset_id = transform_string(*n.children[0]);
        std::vector<IndexType> types;
        if (n.children.size() > 1) {
            types = transform_index_types(*n.children[1]);
        }
        return Command(ReindexCommand(dataset_id, types));
    }
    if (n.is<compact>()) {
        if (n.children[0]->is<all_token>()) {
            return Command(CompactCommand(CompactType::All));
        }
        return Command(CompactCommand(CompactType::Smart));
    }
    if (n.is<config>()) {
        if (n.children[0]->is<get_token>()) {
            std::vector<std::string> elms;
            for (size_t i = 1; i < n.children.size(); i++) {
                elms.push_back(transform_string(*n.children[i]));
            }
            return Command(ConfigGetCommand(elms));
        }
        std::string key = transform_string(*n.children[1]);
        uint64_t value = std::stoi(n.children[2]->content());
        return Command(ConfigSetCommand(key, value));
    }
    if (n.is<status>()) {
        return Command(StatusCommand());
    }
    if (n.is<topology>()) {
        return Command(TopologyCommand());
    }
    if (n.is<ping>()) {
        return Command(PingCommand());
    }
    if (n.is<dataset>()) {
        std::string dataset = transform_string(*n.children[0]);

        if (n.children[1]->is<taint_token>()) {
            std::string taint = transform_string(*n.children[2]);
            return Command(TaintCommand(dataset, TaintMode::Add, taint));
        }
        if (n.children[1]->is<untaint_token>()) {
            std::string taint = transform_string(*n.children[2]);
            return Command(TaintCommand(dataset, TaintMode::Clear, taint));
        }
        if (n.children[1]->is<drop_token>()) {
            return Command(DatasetDropCommand(dataset));
        }
        throw std::runtime_error("Unknown dataset operation node");
    }

    throw std::runtime_error(
        "Unknown parse_tree node, can not create Command.");
}
}  // namespace queryparse

Command parse_command(const std::string &s) {
    string_input<> in(s, "query");

    if (const auto root =
            parse_tree::parse<queryparse::grammar, queryparse::store>(in)) {
        return queryparse::transform_command(*root->children[0]);
    }
    throw std::runtime_error("PARSE FAILED");
}
