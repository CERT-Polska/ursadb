#pragma once

#include "lib/pegtl.hpp"
#include "lib/pegtl/contrib/abnf.hpp"
#include "lib/pegtl/contrib/parse_tree.hpp"
#include "lib/pegtl/contrib/unescape.hpp"

using namespace tao::TAO_PEGTL_NAMESPACE; // NOLINT

namespace queryparse {
struct xdigit : abnf::HEXDIG {};
struct unicode : list<seq<one<'u'>, rep<4, must<xdigit>>>, one<'\\'>> {};
struct escaped_x : seq<one<'x'>, rep<2, must<xdigit>>> {};

struct escaped : sor<escaped_x, unicode> {};
struct character
    : if_must_else<one<'\\'>, escaped, utf8::range<0x20, 0x10FFFF>> {};

struct string_content : until<at<one<'"'>>, must<character>> {};
struct string : seq<one<'"'>, must<string_content>, any> {
    using content = string_content;
};
}
