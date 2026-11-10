#pragma once

#include "urd/cost.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <vector>

namespace urd::detail
{

struct vector_decl
{
    static constexpr std::string_view id = "vector";
    static constexpr auto headers = header_list<1> { "urd/repr/sequences/sequence.hpp" };
    using kind = sequence_kind;
    static constexpr property_set provides = property_set::of({ position::positional, layout::contiguous, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on {};
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    using operation_costs = cost_table<cost_entry<operation::insert, costs::one_a>,
                                       cost_entry<operation::erase, costs::lin_n>,
                                       cost_entry<operation::clear, costs::lin_n>,
                                       cost_entry<operation::iterate, costs::lin_n>,
                                       cost_entry<operation::access, costs::one>>;
    static constexpr double words = 1.5;
    static constexpr std::string_view description = "std::vector";
    static constexpr std::string_view holds = "one array; retains";
};
template<typename T, typename A>
struct declaration_of<std::vector<T, A>>
{
    using declaration = vector_decl;
    static constexpr property_set roles = roles_of<urd::sequence<T>>();
};

}  // namespace urd::detail

template<> struct urd::include<"urd/repr/sequences/sequence.hpp"> {};

namespace urd::detail
{

template<typename S>
struct representation_of<vector_decl, S>
{
    using type = std::vector<typename S::element_type>;
};

}  // namespace urd::detail
