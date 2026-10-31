#pragma once

#include "urd/store/types.hpp"

#include <concepts>
#include <cstddef>
#include <utility>

namespace urd::detail::facet
{

/// A pool of slots: each insert gives a new handle, by which the element is reached, changed and erased. Slots never
/// move, and erased ones are reused, so a handle is valid until its element is erased.
template<typename Derived, typename R, typename S>
class slots
{
    using value_type = typename S::element_type;

public:
    using handle_type = typename R::handle_type;

    handle_type insert(const value_type& element) { return representation().insert(element); }
    handle_type insert(value_type&& element) { return representation().insert(std::move(element)); }
    template<typename... Args>
    handle_type emplace(Args&&... args)
    { return representation().emplace(std::forward<Args>(args)...); }
    /// A slot to use as it is: an erased element with what it held and the storage it kept, else a new one.
    handle_type acquire()
        requires std::default_initializable<value_type>
    { return representation().acquire(); }

    [[nodiscard]] const value_type& operator[](handle_type handle) const { return constant()[handle]; }
    [[nodiscard]] value_type& operator[](handle_type handle) { return representation()[handle]; }

    /// Frees the slot for reuse; the handle and references to the element mean nothing after.
    void erase(handle_type handle)
        requires has<S, shrinkable>
    { representation().erase(handle); }
    void clear()
        requires has<S, shrinkable>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::slots>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return S::is_pool && !has<S, unique>; }
};

}  // namespace urd::detail
