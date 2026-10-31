#pragma once

#include "urd/store/types.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace urd::detail::facet
{

/// Shrinkable: removing, which a Store that only grows (monotone, frozen) or must keep an element (nonempty)
/// does not offer. Removing an element ends the references to it.
template<typename Derived, typename R, typename S>
class removal
{
    using types = store_types<R, S>;
    using value_type = typename types::value_type;
    using iterator_type = typename iterator_types<R, S>::iterator;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;
    static constexpr bool keyed = types::keyed;

public:
    iterator_type erase(const_iterator_type pos)
    {
        // Two conversions, one at a time: the representation's result (perhaps a proxy) to its iterator_type, that
        // to the Store's.
        const std::ranges::iterator_t<R> next = representation().erase(pos);
        return next;
    }
    iterator_type erase(const_iterator_type first, const_iterator_type last) { return representation().erase(first, last); }
    /// Every element equal to `element` (in a map, with that key); how many there were.
    std::size_t erase(const typename types::lookup_type& element)
        requires keyed
    { return representation().erase(element); }
    void pop_back()
        requires(!keyed)
    { representation().pop_back(); }
    void clear() { representation().clear(); }

    /// Removes the elements `pred` holds for; how many. Found by argument-dependent lookup, as std::erase_if.
    template<typename Pred>
    friend std::size_t erase_if(Derived& store, Pred pred)
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
    /// Every element equal to `element`, from a sequence; how many. As std::erase.
    friend std::size_t erase(Derived& store, const value_type& element)
        requires(!keyed)
    {
        return erase_if(store, [&](const value_type& candidate) { return candidate == element; });
    }

private:
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::removal>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, shrinkable> && S::iterable; }
};

}  // namespace urd::detail
