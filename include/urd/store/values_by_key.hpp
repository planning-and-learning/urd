#pragma once

#include "urd/store/types.hpp"

#include <concepts>
#include <utility>

namespace urd::detail::facet
{

/// A map with unique keys: its values, by key. Values change only through these: iterating gives const elements,
/// as some representations' iterators would let a key be overwritten. Changing members need a Store that may
/// change; adding ones a growable Store.
template<typename Derived, typename R, typename S>
class values_by_key
{
    using types = store_types<R, S>;
    using map_key = typename types::key_type;
    using iterator_type = typename iterator_types<R, S>::iterator;

    using mapped = typename types::mapped_type;

public:
    [[nodiscard]] const mapped& at(const map_key& key) const { return representation().at(key); }
    [[nodiscard]] mapped& at(const map_key& key)
        requires(!has<S, frozen>)
    { return representation().at(key); }
    /// The value for `key`, inserted as a default first where there is none.
    mapped& operator[](const map_key& key)
        requires(has<S, growable> && std::default_initializable<mapped>)
    {
        representation().try_emplace(key);
        return representation().at(key);
    }
    /// Inserts a value built from `args` unless `key` has one.
    template<typename... Args>
    std::pair<iterator_type, bool> try_emplace(const map_key& key, Args&&... args)
        requires has<S, growable>
    { return representation().try_emplace(key, std::forward<Args>(args)...); }
    /// Sets the value for `key`, inserting it where there is none.
    template<typename M>
    std::pair<iterator_type, bool> insert_or_assign(const map_key& key, M&& value)
        requires has<S, growable>
    { return representation().insert_or_assign(key, std::forward<M>(value)); }

private:
    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
