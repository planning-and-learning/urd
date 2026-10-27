#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstddef>
#include <ranges>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace urd::detail
{

/// What a representation must guarantee for spec S: S's properties, except where the Store provides them.
/// A frozen Store's elements never move whatever the representation; a sequence's positions are dense
/// in every representation of a sequence.
template<typename S>
struct requirement : S
{
    using reference_category = std::conditional_t<has<S, frozen>, moving_refs, typename S::reference_category>;
    using key_category = std::conditional_t<S::is_sequence, any_keys, typename S::key_category>;
};

/// Whether representation R can serve spec S: it has S's properties for its elements (a set of elements
/// without operator< is not sorted), and the interface a Store for S offers: it places elements as S does (by
/// value or where they are put), is unique exactly where S is (a unique container would drop a sequence's
/// duplicates), and is random-access where S is a sequence.
template<typename R, typename S>
concept serves = satisfies<container_traits<R>, requirement<S>> && conforming<R> && by_value<container_traits<R>> == by_value<S>
                 && has<container_traits<R>, unique> == has<S, unique> && (!has<S, positional> || std::ranges::random_access_range<R>);

/// Container templates, each taking the element type.
template<template<typename...> class... Cs>
struct candidates
{
};

template<typename T>
using SegmentedVector = repr::SegmentedVector<T>;
template<typename T>
using ConcurrentSegmentedVector = repr::SegmentedVector<T, repr::default_first_segment_size, true>;

/// Every representation, in order of preference for S; the first that serves S is chosen. Where elements are
/// few or never change, contiguous storage comes first, as it is the most compact and the fastest to search
/// and iterate; otherwise hashing, then trees, then contiguous storage, which is O(n) to insert. A container
/// is registered by adding it to both lists; what it can serve, its container_traits say.
template<typename S>
using preference = std::conditional_t<
    has<S, frozen> || S::work == workload::small,
    candidates<std::vector, boost::container::flat_multiset, boost::container::flat_set, std::set, std::unordered_set,
               SegmentedVector, ConcurrentSegmentedVector>,
    candidates<std::vector, std::unordered_set, std::set, boost::container::flat_multiset, boost::container::flat_set,
               SegmentedVector, ConcurrentSegmentedVector>>;

template<typename S, typename Candidates = preference<S>>
struct choose;

/// The representation for spec S: the first of the candidates that serves S.
template<typename S, template<typename...> class... Cs>
struct choose<S, candidates<Cs...>>
{
    using element = typename S::element_type;
    static constexpr std::size_t count = sizeof...(Cs);
    static constexpr std::array<bool, count> fits { serves<Cs<element>, S>... };
    static constexpr std::size_t best = []
    {
        std::size_t found = 0;
        while (found < count && !fits[found])
            ++found;
        return found;
    }();

    static_assert(best < count, "urd::Store: no representation provides this combination of properties; "
                                "the pool needs a new representation for it");

    using type = std::tuple_element_t<best < count ? best : 0, std::tuple<Cs<element>...>>;
};

}  // namespace urd::detail
