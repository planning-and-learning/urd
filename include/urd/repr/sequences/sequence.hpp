#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <vector>

namespace urd::detail
{

/// std::vector
struct vector_decl
{
    static constexpr std::string_view id = "vector";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/sequence.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, layout::contiguous, memory::retains, clearing::proportional_clear });
    static constexpr role_tags relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "std::vector";
    static constexpr std::string_view holds = "one array; retains";
};
template<typename T, typename A>
struct declaration_of<std::vector<T, A>>
{
    using declaration = vector_decl;
    static constexpr role_tags roles = element_roles<T>();
};

}  // namespace urd::detail

namespace urd
{

template<>
struct include<"urd/repr/sequences/sequence.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<vector_decl, S>
{
    using type = std::vector<typename S::element_type>;
};

}  // namespace urd::detail
