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

struct xdigit : abnf::HEXDIG {};
struct unicode : list<seq<one<'u'>, rep<4, must<xdigit>>>, one<'\\'>> {};
struct escaped_x : seq<one<'x'>, rep<2, must<xdigit>>> {};
struct escaped_char : one<'"', '\\', '/', 'b', 'f', 'n', 'r', 't'> {};

struct escaped : sor<escaped_char, escaped_x, unicode> {};
struct character : if_must_else<one<'\\'>, escaped, utf8::range<0x20, 0x10FFFF>> {};

struct string_content : until<at<one<'"'>>, must<character>> {};
struct string : seq<one<'"'>, must<string_content>, any> {
    using content = string_content;
};

struct op_and : pad<one<'&'>, space> {};
struct op_or : pad<one<'|'>, space> {};

struct open_bracket : seq<one<'('>, star<space>> {};
struct close_bracket : seq<star<space>, one<')'>> {};

struct open_hex : seq<one<'{'>, star<space>> {};
struct close_hex : seq<star<space>, one<'}'>> {};
struct hexbyte : seq<xdigit, xdigit> {};
struct hexbytes : list<hexbyte, star<space>> {};

struct expression;
struct bracketed : if_must<open_bracket, expression, close_bracket> {};
struct hexstring : if_must<open_hex, hexbytes, close_hex> {};
struct value : sor<string, hexstring, bracketed> {};
struct expression : seq<value, star<sor<op_and, op_or>, expression>> {};

struct select_token : tao::TAO_PEGTL_NAMESPACE::string<'s', 'e', 'l', 'e', 'c', 't'> {};
struct index_token : tao::TAO_PEGTL_NAMESPACE::string<'i', 'n', 'd', 'e', 'x'> {};
struct compact_token : tao::TAO_PEGTL_NAMESPACE::string<'c', 'o', 'm', 'p', 'a', 'c', 't'> {};

struct select : seq<select_token, plus<space>, expression> {};
struct index : seq<index_token, plus<space>, string> {};
struct compact : seq<compact_token> {};

struct command : seq<sor<select, index, compact>, star<space>, one<';'>> {};
struct grammar : seq<command, star<space>, eof> {};

template <typename> struct store : std::false_type {};
template <> struct store<string> : std::true_type {};
template <> struct store<op_and> : parse_tree::remove_content {};
template <> struct store<op_or> : parse_tree::remove_content {};
template <> struct store<expression> : std::true_type {};
template <> struct store<hexstring> : std::true_type {};
template <> struct store<hexbyte> : std::true_type {};
template <> struct store<select> : std::true_type {};
template <> struct store<index> : std::true_type {};
template <> struct store<compact> : std::true_type {};

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

std::map<char, char> char_escape = {
        {'"', '\"'}, {'\\', '\\'}, {'/', '/'}, {'b', '\b'},
        {'f', '\f'}, {'n', '\n'}, {'r', '\r'}, {'t', '\t'}
};

std::string unescape_string(const std::string &str) {
    // at this point, we know that str conforms with the grammar,
    // so no extraordinary checks are needed
    std::string result;
    // string is provided with it's defining quotes
    assert(str[0] == '"');
    assert(str[str.size() - 1] == '"');

    for (int i = 1; i < str.size() - 1; i++) {
        if (str[i] == '\\') {
            auto it = char_escape.find(str.at(i + 1U));

            if (it != char_escape.end()) {
                result += (*it).second;
                i += 1;
                continue;
            }

            if (str.at(i + 1U) != 'x') {
                return result;
            }

            result += (hex2int(str.at(i + 2U)) << 4) + hex2int(str.at(i + 3U));
            i += 3;
        } else {
            result += str[i];
        }
    }
    return result;
}

Query transform(const parse_tree::node &n) {
    if (n.is<string>()) {
        return Query(unescape_string(n.content()));
    } else if (n.is<hexstring>()) {
        std::string result;
        for (auto &hexbyte : n.children) {
            std::string content = hexbyte->content();
            result += (char)((hex2int(content[0]) << 4) + hex2int(content[1]));
        }
        return Query(result);
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
        auto &expr = n.children[0];
        return Command(IndexCommand(unescape_string(expr->content())));
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