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

/// Whether representation R can serve spec S: it has S's properties and the interface a Store for S offers,
/// placing elements the same way (by value or where they are put) and by position where S is a sequence.
template<typename R, typename S>
inline constexpr bool serves = satisfies<container_traits<R>, requirement<S>> && conforming<R>
                               && by_value<container_traits<R>> == by_value<S>
                               && (!has<S, positional> || std::ranges::random_access_range<R>);

/// Whether candidate C fits S. A candidate that does not accept S may name a type that cannot exist (a set of
/// void), so it is not looked at further.
template<typename C, typename S>
consteval bool fits_one()
{
    if constexpr (C::accepts)
        return serves<typename C::type, S>;
    else
        return false;
}

// A candidate registers a representation, as a template over the spec S it is asked for:
//   using type = …;                          // the representation for S
//   static constexpr std::string_view name;  // for messages and the catalog
//   static constexpr bool accepts;           // whether it can represent S at all (kind, element types)
//   static constexpr int rank;               // for S's workload; lower is better

/// The representation for spec S among the candidates Cs: of those that accept S and serve it, the best
/// ranked for S's workload; ties go to the earlier candidate.
template<typename S, template<typename> class... Cs>
struct choose
{
    static constexpr std::size_t count = sizeof...(Cs);

    static constexpr std::array<bool, count> fits { fits_one<Cs<S>, S>()... };
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
