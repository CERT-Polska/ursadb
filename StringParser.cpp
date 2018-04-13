// Based on examples provided by:
// Copyright (c) 2017-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include <iostream>
#include <string>
#include <type_traits>

#include "lib/pegtl.hpp"
#include "lib/pegtl/contrib/unescape.hpp"
#include "lib/pegtl/contrib/parse_tree.hpp"
#include "lib/pegtl/contrib/abnf.hpp"
#include "Query.h"
#include "Parser.h"

using namespace tao::TAO_PEGTL_NAMESPACE;  // NOLINT

namespace queryparse
{
    struct padded : must< string, eof > {};

    // Action class that uses the actions from tao/pegtl/contrib/unescape.hpp to
    // produce a UTF-8 encoded result string where all escape sequences are
    // replaced with their intended meaning.

    template< typename Rule > struct action : nothing< Rule > {};

    template<> struct action< utf8::range< 0x20, 0x10FFFF > > : unescape::append_all {};
    template<> struct action< unicode > : unescape::unescape_u {};
    template<> struct action< escaped_x > : unescape::unescape_x {};
    // clang-format on
}

std::string unescape_string(const std::string &str) {
  unescape::state s;
  string_input<> in(str, "query");
  parse < queryparse::padded, queryparse::action >(in, s);
  return s.unescaped;
}
