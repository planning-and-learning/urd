#pragma once

#include "urd/cost.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace urd::detail::facet
{

/// A map with unique keys: its values, by key. Values change only through these: iterating gives const elements,
/// as some representations' iterators would let a key be overwritten. Changing members need a Store that may
/// change; adding ones a Store that grows.
template<typename Derived, typename R, typename S>
class values_by_key
{
    using map_key = typename S::key_type;
    using iterator_type = typename iterator_types<R, S>::iterator;

public:
    /// What a key maps to.
    using mapped_type = typename S::mapped_type;

    [[nodiscard]] const mapped_type& at(const map_key& key) const { return representation().at(key); }
    [[nodiscard]] mapped_type& at(const map_key& key)
        requires store_types<S>::changeable
    { return representation().at(key); }
    /// The value for `key`, inserted as a default first where there is none.
    mapped_type& operator[](const map_key& key)
        requires(grows<S> && std::default_initializable<mapped_type>)
    {
        representation().try_emplace(key);
        return representation().at(key);
    }
    /// Inserts a value built from `args` unless `key` has one.
    template<typename... Args>
    std::pair<iterator_type, bool> try_emplace(const map_key& key, Args&&... args)
        requires grows<S>
    { return representation().try_emplace(key, std::forward<Args>(args)...); }
    /// Sets the value for `key`, inserting it where there is none.
    template<typename M>
    std::pair<iterator_type, bool> insert_or_assign(const map_key& key, M&& value)
        requires grows<S>
    { return representation().insert_or_assign(key, std::forward<M>(value)); }

    // update and visit: of the maps that reach a value themselves, an index map (urd::dense: the slot of a key, kept
    // across clears) and a concurrent one (urd::concurrent: under its lock).
    /// Calls update(value, held) on the value of `key`, inserting a default one first where there is none (held is then
    /// false), and returns what update returns. A map cleared by generation hands over the value it kept from before
    /// the clear, storage and all, for update to reuse. With concurrent, threads may update at once.
    template<typename Update>
        requires std::invocable<Update&, mapped_type&, bool>
    decltype(auto) update(const map_key& key, Update update)
        requires(grows<S> && std::default_initializable<mapped_type> && (has<R, urd::dense> || has<R, urd::concurrent>))
    { return representation().update(key, update); }
    /// f(value) where `key` has a value, else `missing`: the visit of Boost's concurrent maps, giving what f gives. With
    /// concurrent, safe while others update.
    template<typename F, typename Result>
        requires std::invocable<F&, const mapped_type&>
    [[nodiscard]] Result visit(const map_key& key, F f, Result missing) const
        requires(has<R, urd::dense> || has<R, urd::concurrent>)
    { return representation().visit(key, f, missing); }

    /// Inserts `value` where `key` has none, else replaces the value by combine(value held, value): a hull of
    /// intervals, the lower of two costs. Whether the Store changed, as fixpoints ask. Next to insert_or_assign.
    template<typename Combine>
        requires std::invocable<Combine&, const mapped_type&, const mapped_type&>
    bool insert_or_combine(const map_key& key, const mapped_type& value, Combine combine)
        requires grows<S>
    {
        if (representation().try_emplace(key, value).second)
            return true;
        auto& held = representation().at(key);
        auto combined = mapped_type(combine(std::as_const(held), value));
        if (EqualTo<mapped_type> {}(combined, held))
            return false;
        held = std::move(combined);
        return true;
    }

private:
    [[nodiscard]] const R& representation() const { return store_access::of<Derived>(*this); }
    [[nodiscard]] R& representation() { return store_access::of<Derived>(*this); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet::values_by_key> : facet_defaults
{
    template<typename R, typename S>
    static constexpr bool on()
    { return has<S, unique>; }
};
template<>
struct facet_operations<facet::values_by_key>
{
    using offered = operation_list<operation::lookup>;
};

}  // namespace urd::detail
