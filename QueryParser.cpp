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
// clang-format off

    struct op_and : pad< seq < one< '&' >, one < '&' > >, space > {};
    struct op_or : pad< seq < one< '|' >, one < '|' > >, space > {};

    struct open_bracket : seq< one< '(' >, star< space > > {};
    struct close_bracket : seq< star< space >, one< ')' > > {};

    struct expression;
    struct bracketed : if_must< open_bracket, expression, close_bracket > {};
    struct value : sor< string, bracketed >{};
    struct product : list_must< value, sor< op_and, op_or > > {};
    struct expression : product {};

    struct grammar : seq< expression, eof > {};

    // select which rules in the grammar will produce parse tree nodes:

    // by default, nodes are not generated/stored
    template< typename > struct store : std::false_type {};

    // select which rules in the grammar will produce parse tree nodes:
    template<> struct store< string > : std::true_type {};

    template<> struct store< op_and > : parse_tree::remove_content {};
    template<> struct store< op_or > : parse_tree::remove_content {};

    // after a node is stored successfully, you can add an optional transformer like this:
    struct rearrange : std::true_type
    {
        // recursively rearrange nodes. the basic principle is:
        //
        // from:          PROD/EXPR
        //                /   |   \          (LHS... may be one or more children, followed by OP,)
        //             LHS... OP   RHS       (which is one operator, and RHS, which is a single child)
        //
        // to:               OP
        //                  /  \             (OP now has two children, the original PROD/EXPR and RHS)
        //         PROD/EXPR    RHS          (Note that PROD/EXPR has two fewer children now)
        //             |
        //            LHS...
        //
        // if only one child is left for LHS..., replace the PROD/EXPR with the child directly.
        // otherwise, perform the above transformation, then apply it recursively until LHS...
        // becomes a single child, which then replaces the parent node and the recursion ends.
        static void transform( std::unique_ptr< parse_tree::node >& n )
        {
            if( n->size() == 1 ) {
                n = std::move( n->back() );
            }
            else {
                auto& c = n->children;
                auto r = std::move( c.back() );
                c.pop_back();
                auto o = std::move( c.back() );
                c.pop_back();
                o->children.emplace_back( std::move( n ) );
                o->children.emplace_back( std::move( r ) );
                n = std::move( o );
                transform( n->front() );
            }
        }
    };

    // clang-format off
    template<> struct store< expression > : rearrange {};
// clang-format on

// debugging/show result:

void print_node(const parse_tree::node &n, const std::string &s = "") {
    // detect the root node:
    if (n.is_root()) {
        std::cout << "ROOT" << std::endl;
    } else {
        if (n.has_content()) {
            std::cout << s << n.name() << " \"" << n.content() << "\" at " << n.begin() << " to "
                      << n.end() << std::endl;
        } else {
            std::cout << s << n.name() << " at " << n.begin() << std::endl;
        }
    }
    // print all child nodes
    if (!n.children.empty()) {
        const auto s2 = s + "  ";
        for (auto &up : n.children) {
            print_node(*up, s2);
        }
    }
}

Query transform(const parse_tree::node &n) {
    if (n.has_content()) {
        return Query(unescape_string(n.content()));
    }

    std::vector<Query> queries;

    for (auto &up : n.children) {
        queries.push_back(transform(*up));
    }

    QueryType qt;

    if (n.name() == "queryparse::op_or") {
        qt = QueryType::OR;
    } else if (n.name() == "queryparse::op_and") {
        qt = QueryType::AND;
    } else {
        throw std::runtime_error("encountered unexpected node");
    }

    return Query(qt, queries);
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
