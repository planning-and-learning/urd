#pragma once

#include "urd/properties.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/repr/vector.hpp"

#include <array>
#include <cstddef>
#include <tuple>

namespace urd::detail
{

/// What a representation must guarantee by itself. The rest a Store enforces through its interface:
/// nonempty, frozen and monotone by the operations it offers, the kind (positional, functional, multi) by
/// the representations that accept it.
inline constexpr props guaranteed_by_representation = props::sorted | props::unique | props::stable_refs
    | props::persistent | props::contiguous | props::canonical | props::dense | props::sparse | props::concurrent;

/// Whether a Store of spec S offers erase(), so its representation must be able to remove elements.
template<typename S>
inline constexpr bool needs_erase =
    !includes(S::flags, props::frozen) && !includes(S::flags, props::monotone) && !includes(S::flags, props::nonempty);

/// The representation for spec S among the candidates Cs: of those that accept S, guarantee every
/// requested property and support the operations S's Store offers, the best ranked for S's workload; ties
/// go to the earlier candidate.
template<typename S, template<typename> class... Cs>
struct choose
{
    static constexpr std::size_t count = sizeof...(Cs);
    static constexpr props required = S::flags & guaranteed_by_representation;
    // Once nothing changes, nothing moves either.
    static constexpr props granted = includes(S::flags, props::frozen) ? props::stable_refs : props::none;

    static constexpr std::array<bool, count> fits { (Cs<S>::accepts && includes(Cs<S>::guarantees | granted, required)
                                                      && (Cs<S>::erases || !needs_erase<S>))... };
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
using registry = choose<S, repr::sequence_vector, repr::segmented_sequence, repr::hash_set, repr::tree_set, repr::flat_set>;

}  // namespace urd::detail
