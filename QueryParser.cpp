// Based on examples provided by:
// Copyright (c) 2017-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include <iostream>
#include <string>
#include <type_traits>

#include "lib/pegtl.hpp"
#include "lib/pegtl/contrib/abnf.hpp"
#include "lib/pegtl/contrib/parse_tree.hpp"
#include "lib/pegtl/contrib/unescape.hpp"

#include "Parser.h"
#include "Query.h"
#include "StringParser.h"

using namespace tao::TAO_PEGTL_NAMESPACE; // NOLINT

namespace queryparse {

struct op_and : pad<one<'&'>, space> {};
struct op_or : pad<one<'|'>, space> {};

struct open_bracket : seq<one<'('>, star<space>> {};
struct close_bracket : seq<star<space>, one<')'>> {};

struct expression;
struct bracketed : if_must<open_bracket, expression, close_bracket> {};
struct value : sor<string, bracketed> {};
struct expression : seq<value, star<sor<op_and, op_or>, expression>> {};

struct grammar : seq<expression, eof> {};

template <typename> struct store : std::false_type {};
template <> struct store<string> : std::true_type {};

template <> struct store<op_and> : parse_tree::remove_content {};
template <> struct store<op_or> : parse_tree::remove_content {};
template <> struct store<expression> : std::true_type {};

void print_node(const parse_tree::node &n, const std::string &s = "") {
    if (n.has_content()) {
        std::cout << s << n.name() << " \"" << n.content() << "\" at " << n.begin() << " to "
                  << n.end() << std::endl;
    } else {
        std::cout << s << n.name() << " at " << n.begin() << std::endl;
    }
    if (!n.children.empty()) {
        for (auto &up : n.children) {
            print_node(*up, s + "  ");
        }
    }
}

Query transform(const parse_tree::node &n) {
    if (n.is<string>()) {
        return Query(unescape_string(n.content()));
    } else if (n.is<expression>()) {
        if (n.children.size() == 1) {
            return transform(*n.children[0]);
        }
        auto &expr = n.children[1];
        if (expr->is<op_or>()) {
            return Query(QueryType::OR, {transform(*n.children[0]), transform(*n.children[2])});
        } else if (expr->is<op_and>()) {
            return Query(QueryType::AND, {transform(*n.children[0]), transform(*n.children[2])});
        } else {
            throw std::runtime_error("encountered unexpected expression");
        }
    }
    throw std::runtime_error("encountered unexpected node");
}
}

Query parse_query(const std::string &s) {
    string_input<> in(s, "query");

    if (const auto root = parse_tree::parse<queryparse::grammar, queryparse::store>(in)) {
        queryparse::print_node(*root->children[0]);
        return queryparse::transform(*root->children[0]);
    } else {
        throw std::runtime_error("PARSE FAILED");
    }
}