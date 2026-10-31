#pragma once

// The registry's table: every declaration (urd/registry/declarations.hpp) as values, one entry each, in order of
// preference.

#include "urd/registry/declarations.hpp"

#include <array>

namespace urd::detail
{

template<typename... Decls>
constexpr auto entries_from(type_list<Decls...> /*declarations*/)
{ return std::array<registry_entry, sizeof...(Decls)> { entry_of<Decls>()... }; }

/// The table as values, one entry per declaration, in the same order.
inline constexpr auto registry_table = entries_from(declarations {});

}  // namespace urd::detail
