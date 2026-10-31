#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/store/types.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
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
    /// Ordered by its values (order_by<P> with P(key, value)): a value changes only through what puts it back in order.
    static constexpr bool ordered_by_value = is_entry_less<typename S::order_type>;

public:
    [[nodiscard]] const mapped& at(const map_key& key) const { return representation().at(key); }
    [[nodiscard]] mapped& at(const map_key& key)
        requires(!has<S, frozen> && !ordered_by_value)
    { return representation().at(key); }
    /// The value for `key`, inserted as a default first where there is none.
    mapped& operator[](const map_key& key)
        requires(has<S, growable> && std::default_initializable<mapped> && !ordered_by_value)
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

    /// Calls update(value, held) on the value of `key`, inserting a default one first where there is none (held is then
    /// false), and returns what update returns. A map cleared by generation hands over the value it kept from before
    /// the clear, storage and all, for update to reuse. With concurrent, threads may update at once.
    template<typename Update>
        requires std::invocable<Update&, mapped&, bool>
    decltype(auto) update(const map_key& key, Update update)
        requires(has<S, growable> && std::default_initializable<mapped>)
    {
        if constexpr (requires { representation().update(key, update); })
            return representation().update(key, update);
        else if constexpr (locks_by_key)
            return locked_update(key, update);
        else
        {
            const bool held = !representation().try_emplace(key).second;
            return update(representation().at(key), held);
        }
    }
    /// read(value) where `key` has a value, else `missing`. With concurrent, safe while others update.
    template<typename Read, typename Result>
        requires std::invocable<Read&, const mapped&>
    [[nodiscard]] Result read(const map_key& key, Read read, Result missing) const
    {
        if constexpr (requires { representation().read(key, read, missing); })
            return representation().read(key, read, missing);
        else if constexpr (locks_by_key)
        {
            auto result = std::move(missing);
            representation().if_contains(key, [&read, &result](const auto& entry) { result = Result(read(entry.second)); });
            return result;
        }
        else
            return representation().contains(key) ? Result(read(representation().at(key))) : missing;
    }

    /// Inserts `value` where `key` has none, else replaces the value by combine(value held, value): a hull of
    /// intervals, the lower of two costs. Whether the Store changed, as fixpoints ask.
    template<typename Combine>
        requires std::invocable<Combine&, const mapped&, const mapped&>
    bool merge(const map_key& key, const mapped& value, Combine combine)
        requires has<S, growable>
    {
        if constexpr (ordered_by_value)
        {
            // Through update, which puts the entry back in order.
            auto changed = false;
            representation().update(key,
                                    [&](mapped& held, bool had)
                                    {
                                        auto next = had ? mapped(combine(std::as_const(held), value)) : value;
                                        changed = !had || !EqualTo<mapped> {}(next, held);
                                        held = std::move(next);
                                    });
            return changed;
        }
        else
        {
            if (representation().try_emplace(key, value).second)
                return true;
            auto& held = representation().at(key);
            auto combined = mapped(combine(std::as_const(held), value));
            if (EqualTo<mapped> {}(combined, held))
                return false;
            held = std::move(combined);
            return true;
        }
    }

private:
    /// The representation changes and reads a key's value under a lock of its own, as gtl's sharded tables do with
    /// lazy_emplace_l and if_contains: update and read then go through them, safe while other threads do the same.
    static constexpr bool locks_by_key = requires(R& table, const R& constant, const map_key& key) {
        table.lazy_emplace_l(key, [](auto&) {}, [](const auto&) {});
        constant.if_contains(key, [](const auto&) {});
    };

    /// update(value, held) under the key's lock; a new value is built and updated before it goes in, still under it.
    template<typename Update>
    auto locked_update(const map_key& key, Update& update)
    {
        using Result = std::invoke_result_t<Update&, mapped&, bool>;
        static_assert(!std::is_reference_v<Result>, "urd::Store::update: a reference would outlive the lock of the key");
        if constexpr (std::is_void_v<Result>)
            representation().lazy_emplace_l(
                key,
                [&update](auto& entry) { update(entry.second, true); },
                [&key, &update](const auto& construct)
                {
                    auto value = mapped();
                    update(value, false);
                    construct(key, std::move(value));
                });
        else
        {
            auto result = std::optional<Result>();
            representation().lazy_emplace_l(
                key,
                [&update, &result](auto& entry) { result.emplace(update(entry.second, true)); },
                [&key, &update, &result](const auto& construct)
                {
                    auto value = mapped();
                    result.emplace(update(value, false));
                    construct(key, std::move(value));
                });
            return std::move(*result);
        }
    }

    [[nodiscard]] const R& representation() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    [[nodiscard]] R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::values_by_key>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return S::is_map && has<S, unique>; }
};

}  // namespace urd::detail
