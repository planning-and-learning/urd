#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace urd::detail::facet
{

/// Shrinkable: removing, which a Store that only grows (monotone, frozen) or must keep an element (nonempty)
/// does not offer; one that only grows until cleared (monotone_until_clear) offers clear() alone. Removing an element
/// ends the references to it.
template<typename Derived, typename R, typename S>
class removal
{
    using types = store_types<S>;
    using value_type = typename types::value_type;
    using iterator_type = typename iterator_types<R, S>::iterator;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;

public:
    iterator_type erase(const_iterator_type pos)
        requires changes_in_the_middle<S>
    {
        // Two conversions, one at a time: the representation's result (perhaps a proxy) to its iterator_type, that
        // to the Store's.
        const std::ranges::iterator_t<R> next = representation().erase(pos);
        return next;
    }
    iterator_type erase(const_iterator_type first, const_iterator_type last)
        requires changes_in_the_middle<S>
    { return representation().erase(first, last); }
    /// Every element equal to `element` (in a map, with that key); how many there were. Where the kind places elements
    /// by their value or key (it does not append): one erase with the other erases, as std::set has it.
    std::size_t erase(const typename types::lookup_type& element)
        requires(shrinks<S> && !appends<typename S::kind>)
    { return representation().erase(element); }
    void clear() { representation().clear(); }

    /// Removes the elements `pred` holds for; how many. Found by argument-dependent lookup, as std::erase_if.
    template<typename Pred>
    friend std::size_t erase_if(Derived& store, Pred pred)
        requires changes_in_the_middle<S>
    {
        const auto before = store.size();
        if constexpr (types::writable)
        {
            auto& range = store_access::representation(store);
            range.erase(std::remove_if(range.begin(), range.end(), pred), range.end());
        }
        else
            // ponytail: one erase per element, O(n²) on a flat set; erase_if of the representation if it matters
            for (auto it = store.begin(); it != store.end();)
                it = pred(*it) ? store.erase(it) : std::next(it);
        return before - store.size();
    }

private:
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::removal> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return clears<S>; }
};
template<>
struct facet_operations<facet::removal>
{
    using offered = operation_list<operation::erase, operation::clear>;
};

}  // namespace urd::detail
