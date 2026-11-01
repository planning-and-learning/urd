#pragma once

// The registry's tables: each kind's declarations (urd/registry/declarations.hpp, declarations_of) as values, one
// entry each, in order of preference.

#include "urd/registry/declarations.hpp"

#include <array>

namespace urd::detail
{

template<typename... Decls>
constexpr auto entries_from(type_list<Decls...> /*declarations*/)
{ return std::array<registry_entry, sizeof...(Decls)> { entry_of<Decls>()... }; }

/// A list of declarations as values, one entry each, in the same order; and a kind's table (kinds sharing their
/// containers share one).
template<typename List>
inline constexpr auto table_of_list = entries_from(List {});
template<typename Kind>
inline constexpr const auto& table_of = table_of_list<declarations_t<Kind>>;

}  // namespace urd::detail
