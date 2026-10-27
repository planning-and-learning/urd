#pragma once

#include "urd/properties.hpp"
#include "urd/repr/ops.hpp"

#include <boost/container/flat_set.hpp>

#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <vector>

namespace urd::repr
{

/// Boost's flat sets: sorted vectors that keep themselves sorted (and, flat_set, free of duplicates).
template<typename R, typename E, auto ordered>
struct boost_flat_ops
{
    [[nodiscard]] static R make(std::vector<E> elements) { return R(elements.begin(), elements.end()); }
    /// Takes the elements as already in order, as the Store's assume() has checked in Debug builds.
    [[nodiscard]] static R assume(std::vector<E> elements) { return R(ordered, elements.begin(), elements.end()); }
    static bool insert(R& range, const E& element)
    {
        if constexpr (std::is_same_v<decltype(range.insert(element)), typename R::iterator>)
        {
            range.insert(element);
            return true;
        }
        else
            return range.insert(element).second;
    }
    static std::size_t erase(R& range, const E& element) { return range.erase(element); }
    static void clear(R& range) noexcept { range.clear(); }
    [[nodiscard]] static bool contains(const R& range, const E& element) { return range.contains(element); }
    [[nodiscard]] static const E& at(const R& range, std::size_t pos) { return *range.nth(pos); }
};

template<typename K>
struct ops<boost::container::flat_set<K>> : boost_flat_ops<boost::container::flat_set<K>, K, boost::container::ordered_unique_range>
{
};

template<typename K>
struct ops<boost::container::flat_multiset<K>> : boost_flat_ops<boost::container::flat_multiset<K>, K, boost::container::ordered_range>
{
};

/// A sorted sequence: boost::container::flat_multiset, or flat_set when it is also unique.
template<typename S>
struct sorted_sequence
{
    using type = std::conditional_t<includes(S::flags, props::unique), boost::container::flat_set<typename S::value_type>,
                                    boost::container::flat_multiset<typename S::value_type>>;
    static constexpr std::string_view name = includes(S::flags, props::unique) ? "boost::container::flat_set"
                                                                                : "boost::container::flat_multiset";
    static constexpr bool erases = true;
    static constexpr bool accepts = S::is_sequence && includes(S::flags, props::sorted) && std::totally_ordered<typename S::value_type>;
    static constexpr props guarantees = props::sorted | props::contiguous | props::dense
        | (includes(S::flags, props::unique) ? props::unique | props::canonical : props::none);
    static constexpr int rank = 0;
};

/// A set as a sorted vector without duplicates: compact and fast to search, O(n) to insert.
template<typename S>
struct flat_set
{
    using type = boost::container::flat_set<typename S::key_type>;
    static constexpr std::string_view name = "boost::container::flat_set";
    static constexpr bool erases = true;
    static constexpr bool accepts = S::is_set && std::totally_ordered<typename S::key_type>;
    static constexpr props guarantees =
        props::sorted | props::unique | props::contiguous | props::canonical | props::sparse;
    static constexpr int rank =
        includes(S::flags, props::frozen) || S::work == workload::small ? 0 : (S::work == workload::many_similar ? 3 : 2);
};

}  // namespace urd::repr
