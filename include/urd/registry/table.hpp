#pragma once

// The registry's table: every declaration (urd/registry/declarations.hpp), urd's then a downstream project's, as values,
// one entry each, in order of preference.

#include "urd/registry/declarations.hpp"

#include <array>

namespace urd::detail
{

template<typename... Decls>
constexpr auto entries_of(type_list<Decls...> /*declarations*/)
{ return std::array<registry_entry, sizeof...(Decls)> { entry_of<Decls>()... }; }

/// Every declaration, urd's then the user's.
using all_declarations = typename join<declarations, typename user_declarations<>::type>::type;
/// The table as values, one entry per declaration, in the same order.
inline constexpr auto registry_table = entries_of(all_declarations {});

}  // namespace urd::detail
