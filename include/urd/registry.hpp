#pragma once

#include "urd/properties.hpp"
#include "urd/repr/flat.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

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

/// The representation for spec S among the candidates Cs: of those that accept S and whose container_traits
/// satisfy S's requirement, the best ranked for S's workload; ties go to the earlier candidate.
template<typename S, template<typename> class... Cs>
struct choose
{
    static constexpr std::size_t count = sizeof...(Cs);

    static constexpr std::array<bool, count> fits { (
        Cs<S>::accepts && satisfies<container_traits<typename Cs<S>::type>, requirement<S>>)... };
    static constexpr std::array<int, count> ranks { Cs<S>::rank... };

    static constexpr std::size_t best = []
    {
        auto found = count;
        for (std::size_t pos = 0; pos < count; ++pos)
            if (fits[pos] && (found == count || ranks[pos] < ranks[found]))
                found = pos;
        return found;
    }();

    static_assert(best < count, "urd::Store: no representation provides this combination of properties; "
                                "the pool needs a new representation for it");

    using type = std::tuple_element_t<best < count ? best : 0, std::tuple<Cs<S>...>>;
};

/// Every representation a Store can resolve to.
template<typename S>
using registry = choose<S, repr::sequence_vector, repr::sorted_sequence, repr::segmented_sequence, repr::hash_set, repr::tree_set, repr::flat_set>;

}  // namespace urd::detail
