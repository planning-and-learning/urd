#pragma once

#include "urd/properties.hpp"
#include "urd/traits.hpp"

#include <boost/container/flat_set.hpp>

#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>

namespace urd
{

// Boost's flat sets order by std::less by default; like std::set, only that is `sorted`.
template<typename K, typename Allocator>
struct container_traits<boost::container::flat_set<K, std::less<K>, Allocator>>
    : properties_of<sorted, unique, sparse, contiguous, canonical, shrinkable>
{
};

template<typename K, typename Allocator>
struct container_traits<boost::container::flat_multiset<K, std::less<K>, Allocator>>
    : properties_of<sorted, contiguous, canonical, shrinkable>
{
};

}  // namespace urd

namespace urd::repr
{

/// A sorted sequence: boost::container::flat_multiset, or flat_set when it is also unique.
template<typename S>
struct sorted_sequence
{
    using type = std::conditional_t<has<S, unique>, boost::container::flat_set<typename S::value_type>,
                                    boost::container::flat_multiset<typename S::value_type>>;
    static constexpr std::string_view name = has<S, unique> ? "boost::container::flat_set" : "boost::container::flat_multiset";
    static constexpr bool accepts = S::is_sequence && has<S, sorted> && std::totally_ordered<typename S::value_type>;
    static constexpr int rank = 0;
};

/// A set as a sorted vector without duplicates: compact and fast to search, O(n) to insert.
template<typename S>
struct flat_set
{
    using type = boost::container::flat_set<typename S::key_type>;
    static constexpr std::string_view name = "boost::container::flat_set";
    static constexpr bool accepts = S::is_set && std::totally_ordered<typename S::key_type>;
    static constexpr int rank =
        has<S, frozen> || S::work == workload::small ? 0 : (S::work == workload::many_similar ? 3 : 2);
};

}  // namespace urd::repr
