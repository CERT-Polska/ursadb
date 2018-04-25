// Based on examples provided by:
// Copyright (c) 2017-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include "QueryParser.h"

#include <iostream>
#include <string>
#include <type_traits>

#include "lib/pegtl.hpp"
#include "lib/pegtl/contrib/abnf.hpp"
#include "lib/pegtl/contrib/parse_tree.hpp"
#include "lib/pegtl/contrib/unescape.hpp"

#include "Command.h"
#include "Query.h"

using namespace tao::TAO_PEGTL_NAMESPACE; // NOLINT

namespace queryparse {

struct hexdigit : abnf::HEXDIG {};
struct hexbyte : seq<hexdigit, hexdigit> {};
struct escaped_x : seq<one<'x'>, hexbyte> {};
struct escaped_char : one<'"', '\\', 'b', 'f', 'n', 'r', 't'> {};
struct escaped : sor<escaped_char, escaped_x> {};
struct ascii_char : utf8::range<0x20, 0x7e> {};
struct character : if_must_else<one<'\\'>, escaped, ascii_char> {};
struct string_content : until<at<one<'"'>>, must<character>> {};
struct plaintext : seq<one<'"'>, must<string_content>, any> {
    using content = string_content;
};
struct wide_plaintext : seq<one<'w'>, plaintext> {};
struct op_and : pad<one<'&'>, space> {};
struct op_or : pad<one<'|'>, space> {};
struct open_bracket : seq<one<'('>, star<space>> {};
struct close_bracket : seq<star<space>, one<')'>> {};
struct open_curly : seq<one<'{'>, star<space>> {};
struct close_curly : seq<star<space>, one<'}'>> {};
struct open_square : seq<one<'['>, star<space>> {};
struct close_square : seq<star<space>, one<']'>> {};

struct hexbytes : list<hexbyte, star<space>> {};
struct hexstring : if_must<open_curly, hexbytes, close_curly> {};
struct string_like : sor<wide_plaintext, plaintext, hexstring> {};
struct expression;
struct bracketed : if_must<open_bracket, expression, close_bracket> {};
struct value : sor<string_like, bracketed> {};
struct expression : seq<value, star<sor<op_and, op_or>, expression>> {};
struct comma_token : one<','> {};
struct select_token : string<'s', 'e', 'l', 'e', 'c', 't'> {};
struct index_token : string<'i', 'n', 'd', 'e', 'x'> {};
struct compact_token : string<'c', 'o', 'm', 'p', 'a', 'c', 't'> {};
struct with_token : string<'w', 'i', 't', 'h'> {};
struct gram3_token : string<'g', 'r', 'a', 'm', '3'> {};
struct hash4_token : string<'h', 'a', 's', 'h', '4'> {};
struct text4_token : string<'t', 'e', 'x', 't', '4'> {};
struct wide8_token : string<'w', 'i', 'd', 'e', '8'> {};
struct comma : seq<star<space>, comma_token, star<space>> {};
struct index_type : sor<gram3_token, hash4_token, text4_token, wide8_token> {};
struct index_type_list : seq<open_square, opt<list<index_type, comma>>, close_square> {};
struct index_with_construct : seq<with_token, star<space>, index_type_list> {};
struct select : seq<select_token, star<space>, expression> {};
struct index : seq<index_token, star<space>, string_like, star<space>, opt<index_with_construct>> {
};
struct compact : seq<compact_token> {};
struct command : seq<sor<select, index, compact>, star<space>, one<';'>> {};
struct grammar : seq<command, star<space>, eof> {};

template <typename> struct store : std::false_type {};
template <> struct store<plaintext> : std::true_type {};
template <> struct store<wide_plaintext> : std::true_type {};
template <> struct store<op_and> : parse_tree::remove_content {};
template <> struct store<op_or> : parse_tree::remove_content {};
template <> struct store<expression> : std::true_type {};
template <> struct store<hexstring> : std::true_type {};
template <> struct store<escaped_char> : std::true_type {};
template <> struct store<hexbyte> : std::true_type {};
template <> struct store<ascii_char> : std::true_type {};
template <> struct store<select> : std::true_type {};
template <> struct store<index> : std::true_type {};
template <> struct store<compact> : std::true_type {};
template <> struct store<index_type_list> : std::true_type {};
template <> struct store<gram3_token> : std::true_type {};
template <> struct store<hash4_token> : std::true_type {};
template <> struct store<text4_token> : std::true_type {};
template <> struct store<wide8_token> : std::true_type {};

constexpr int hex2int(char hexchar) {
    if (hexchar >= '0' && hexchar <= '9') {
        return hexchar - '0';
    } else if (hexchar >= 'a' && hexchar <= 'f') {
        return 10 + hexchar - 'a';
    } else if (hexchar >= 'A' && hexchar <= 'F') {
        return 10 + hexchar - 'A';
    } else {
        throw std::runtime_error("invalid hex char");
    }
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
    } else if (n.is<ascii_char>()) {
        return content[0];
    } else if (n.is<escaped_char>()) {
        return unescape_char(content[0]);
    } else {
        throw std::runtime_error("unknown character parse");
    }
}

std::vector<IndexType> transform_index_types(const parse_tree::node &n) {
    std::vector<IndexType> result;
    for (auto &child : n.children) {
        auto type = index_type_from_string(child->content());
        if (type == std::nullopt) {
            throw std::runtime_error("index type unsupported by parser");
        }
        result.push_back(type.value());
    }
    return result;
}

std::string transform_string(const parse_tree::node &n) {
    auto *root = &n;

    if (n.is<wide_plaintext>()) {
        // unpack plaintext from inside wide_plaintext
        root = n.children[0].get();
    }

    std::string result;
    for (auto &atom : root->children) {
        result += transform_char(*atom);

        if (n.is<wide_plaintext>()) {
            result += (char)0;
        }
    }
    return result;
}

Query transform(const parse_tree::node &n) {
    if (n.is<plaintext>() || n.is<wide_plaintext>() || n.is<hexstring>()) {
        return Query(transform_string(n));
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

Command transform_command(const parse_tree::node &n) {
    if (n.is<select>()) {
        auto &expr = n.children[0];
        return Command(SelectCommand(transform(*expr)));
    } else if (n.is<index>()) {
        std::string path = transform_string(*n.children[0]);
        std::vector<IndexType> types = IndexCommand::default_types();
        if (n.children.size() > 1) {
            types = transform_index_types(*n.children[1]);
        }
        return Command(IndexCommand(path, types));
    } else if (n.is<compact>()) {
        return Command(CompactCommand());
    }

    throw std::runtime_error("Unknown parse_tree node, can not create Command.");
}
}

Command parse_command(const std::string &s) {
    string_input<> in(s, "query");

    if (const auto root = parse_tree::parse<queryparse::grammar, queryparse::store>(in)) {
        return queryparse::transform_command(*root->children[0]);
    } else {
        throw std::runtime_error("PARSE FAILED");
    }
}