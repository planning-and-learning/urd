#pragma once

// Explicit instantiations of the conversions from Python of a context's children (urd::python::detail::conversions,
// bindings.hpp): the one source of a binding library that instantiates them for a context C of kinds Types (a
// boost::mp11 list, named without commas: an alias),
//
//     URD_PYTHON_CONVERSIONS(, C, Types, Count)
//
// and its other sources, which declare them (PREFIX extern) where they bind builders, link to these.

#include "urd/python/bindings.hpp"

#include <boost/mp11.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage,bugprone-macro-parentheses): the macros expand to explicit instantiations,
// declarations, which no function gives; their arguments are types and a prefix, which no parentheses may enclose
#define URD_PYTHON_CONVERSIONS_AT(z, I, ARGS) \
    BOOST_PP_TUPLE_ELEM(3, 0, ARGS)           \
    template struct ::urd::python::detail::conversions<::boost::mp11::mp_at_c<BOOST_PP_TUPLE_ELEM(3, 2, ARGS), I>, BOOST_PP_TUPLE_ELEM(3, 1, ARGS)>;

/// Explicit instantiations (PREFIX extern: declarations) of the child conversion of each of the Count kinds Types of context
/// Context. The count is checked against the list.
#define URD_PYTHON_CONVERSIONS(PREFIX, Context, Types, Count)                                        \
    static_assert(::boost::mp11::mp_size<Types>::value == (Count), "URD_PYTHON_CONVERSIONS: Count"); \
    BOOST_PP_REPEAT(Count, URD_PYTHON_CONVERSIONS_AT, (PREFIX, Context, Types))
// NOLINTEND(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
