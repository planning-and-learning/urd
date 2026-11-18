#pragma once

#include "urd/cost.hpp"
#include "urd/store/types.hpp"

namespace urd::detail::facet
{

/// Sorted: searching by order.
template<typename Derived, typename R, typename S>
class ordered_lookup
{
    using types = store_types<S>;
    using const_iterator_type = typename iterator_types<R, S>::const_iterator;

public:
    [[nodiscard]] const_iterator_type lower_bound(const typename types::lookup_type& element) const { return representation().lower_bound(element); }

    [[nodiscard]] const_iterator_type upper_bound(const typename types::lookup_type& element) const { return representation().upper_bound(element); }

    /// By another form of a key: forwarded as it is, so what the Store's order compares with an element decides what K
    /// may be (a transparent order: the standard's heterogeneous lookup). A K it does not take does not compile.
    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, typename types::lookup_type>)
    [[nodiscard]] const_iterator_type lower_bound(const K& key) const
    { return representation().lower_bound(key); }

    template<typename K>
        requires(!std::same_as<std::remove_cvref_t<K>, typename types::lookup_type>)
    [[nodiscard]] const_iterator_type upper_bound(const K& key) const
    { return representation().upper_bound(key); }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::ordered_lookup> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, sorted>; }
};

template<>
struct facet_operations<facet::ordered_lookup>
{
    using offered = operation_list<operation::ordered_lookup>;
};

}  // namespace urd::detail
