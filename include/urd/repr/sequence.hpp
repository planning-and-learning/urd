#pragma once

#include "urd/container_synopsis.hpp"
#include "urd/costs.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/registry/features.hpp"
#include "urd/traits.hpp"

#include <vector>

// What this file's containers guarantee, as a function of the facts about the elements they are given (urd/registry/facts.hpp):
// what their declarations below offer, or none where they do not take these elements.
namespace urd::detail::guarantees
{

constexpr P vector() { return props({ tag::dense, tag::positional, tag::contiguous, tag::shrinkable, tag::retains, tag::proportional_clear }); }

}  // namespace urd::detail::guarantees


namespace urd::detail
{

/// std::vector
struct vector_decl
{
    static constexpr std::string_view id = "vector";
    static constexpr auto headers = header_list<1> { "urd/repr/sequence.hpp" };
    static constexpr offered offers(const request_features& f)
    {
        if (!accepts_order(natural_order_only, f.by))
            return {};
        const auto held = guarantees::vector();
        if (!held)
            return {};
        return offer_of(urd::kind::sequence, *held, f);
    }
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::lin_nt;
        static constexpr cost iterate = costs::lin_nt;
        static constexpr cost access = costs::one_t;
        static constexpr double words = 1.5;
    };
    static constexpr std::string_view description = "std::vector";
    static constexpr std::string_view holds = "one array; retains";
};
template<typename T, typename A>
struct declaration_of<std::vector<T, A>>
{
    using declaration = vector_decl;
    static constexpr request_features features = element_facts<T>();
};

}  // namespace urd::detail

namespace urd
{

template<>
struct include<"urd/repr/sequence.hpp">
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
