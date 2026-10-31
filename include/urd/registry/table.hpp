#pragma once

// The registry's tables: each node's declarations (urd/registry/declarations.hpp) as values, one entry each, in order
// of preference.

#include "urd/registry/declarations.hpp"

#include <array>

namespace urd::detail
{

template<typename... Decls>
constexpr auto entries_from(type_list<Decls...> /*declarations*/)
{ return std::array<registry_entry, sizeof...(Decls)> { entry_of<Decls>()... }; }

/// Node's table as values, one entry per declaration, in the same order.
template<typename Node>
inline constexpr auto table_of = entries_from(typename Node::declarations {});

}  // namespace urd::detail
