#pragma once

// For tests/scaling: the registry grown by URD_REGISTRY_COPIES copies of every declaration (none where it is not
// defined), through the extension point a downstream project uses (user_declarations). A copy is checked as the
// declaration it copies is, then serves nothing, so every request resolves as before at the cost a registry that many
// times larger has. Include it before urd/store.hpp.

#include "urd/registry/declarations.hpp"

#include <cstddef>
#include <utility>

#if defined(URD_REGISTRY_COPIES)

namespace urd::test
{

template<std::size_t I, typename Decl>
struct copy_decl
{
    static constexpr std::string_view id = Decl::id;
    static constexpr auto headers = Decl::headers;
    static constexpr detail::offered offers(const detail::request_features& f)
    {
        if (Decl::offers(f))
            return {};
        return {};
    }
};

template<std::size_t I, typename... Decls>
using copies = detail::type_list<copy_decl<I, Decls>...>;
template<typename... Decls, std::size_t... I>
auto all_copies(detail::type_list<Decls...> /*declarations*/, std::index_sequence<I...> /*copies*/) -> typename detail::join<copies<I, Decls...>...>::type;

}  // namespace urd::test

template<>
struct urd::detail::user_declarations<>
{
    using type = decltype(urd::test::all_copies(declarations {}, std::make_index_sequence<URD_REGISTRY_COPIES>()));
};

#endif
