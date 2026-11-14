#pragma once

// gtl's sharded hash maps (flat, or a node per element), defined. Include it where a Store resolves to one.

#include "urd/elements/prehashed.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/gtl_memory.hpp"
#include "urd/repr/maps/parallel_hash_map_fwd.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <gtl/phmap.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

template<>
struct urd::include<"urd/repr/maps/parallel_hash_map.hpp">
{
};

namespace urd::repr
{

/// A gtl sharded table whose update and read run under the lock of the key's shard: safe while other threads do the
/// same.
template<typename Table>
class LockedByKey : private Table
{
public:
    using Table::Table;
    // What the facets reach of gtl's table, and nothing else of it (tests/rules/exposure.py, rule F7).
    using Table::at;
    using Table::begin;
    using Table::clear;
    using Table::contains;
    using Table::count;
    using Table::emplace;
    using Table::empty;
    using Table::end;
    using Table::equal_range;
    using Table::erase;
    using Table::find;
    using Table::insert;
    using Table::insert_or_assign;
    using typename Table::const_iterator;
    using typename Table::const_reference;
    using typename Table::difference_type;
    using typename Table::iterator;
    using typename Table::key_type;
    using typename Table::mapped_type;
    using typename Table::reference;
    using typename Table::size_type;
    using typename Table::value_type;
    using Table::operator[];
    using Table::reserve;
    using Table::size;
    using Table::try_emplace;

    friend bool operator==(const LockedByKey& lhs, const LockedByKey& rhs) { return static_cast<const Table&>(lhs) == static_cast<const Table&>(rhs); }

    /// update(value, held) under the key's lock; a new value is built and updated before it goes in, still under it.
    template<typename Update>
        requires std::invocable<Update&, mapped_type&, bool>
    auto update(const key_type& key, Update& update)
    {
        using Result = std::invoke_result_t<Update&, mapped_type&, bool>;
        static_assert(!std::is_reference_v<Result>, "urd::Store::update: a reference would outlive the lock of the key");
        if constexpr (std::is_void_v<Result>)
            this->lazy_emplace_l(
                key,
                [&update](auto& entry) { update(entry.second, true); },
                [&key, &update](const auto& construct)
                {
                    auto value = mapped_type();
                    update(value, false);
                    construct(key, std::move(value));
                });
        else
        {
            auto result = std::optional<Result>();
            this->lazy_emplace_l(
                key,
                [&update, &result](auto& entry) { result.emplace(update(entry.second, true)); },
                [&key, &update, &result](const auto& construct)
                {
                    auto value = mapped_type();
                    result.emplace(update(value, false));
                    construct(key, std::move(value));
                });
            return std::move(*result);
        }
    }
    /// f(value) where `key` has a value, else `missing`, under the key's lock.
    template<typename F, typename Result>
    [[nodiscard]] Result visit(const key_type& key, F& f, Result missing) const
    {
        auto result = std::move(missing);
        this->if_contains(key, [&f, &result](const auto& entry) { result = Result(f(entry.second)); });
        return result;
    }

private:
    /// Its memory is gtl's table's (urd::memory_of, beside the maps below).
    template<typename Held>
    friend struct urd::memory_of;
    [[nodiscard]] const Table& table() const noexcept { return *this; }
};

template<typename K, typename V>
class ParallelHashMap :
    public LockedByKey<gtl::parallel_flat_hash_map<K,
                                                   V,
                                                   urd::detail::prehash_hash<K>,
                                                   urd::detail::prehash_equal<K>,
                                                   std::allocator<std::pair<const K, V>>,
                                                   shard_bits,
                                                   std::mutex>>
{
public:
    using ParallelHashMap::LockedByKey::LockedByKey;
};

template<typename K, typename V>
class ParallelNodeHashMap :
    public LockedByKey<gtl::parallel_node_hash_map<K,
                                                   V,
                                                   urd::detail::prehash_hash<K>,
                                                   urd::detail::prehash_equal<K>,
                                                   std::allocator<std::pair<const K, V>>,
                                                   shard_bits,
                                                   std::mutex>>
{
public:
    using ParallelNodeHashMap::LockedByKey::LockedByKey;
};

}  // namespace urd::repr

template<typename K, typename V>
struct urd::memory_of<urd::repr::ParallelHashMap<K, V>>
{
    static std::size_t bytes(const repr::ParallelHashMap<K, V>& held) { return detail::sharded_table_bytes(held.table()); }
};
template<typename K, typename V>
struct urd::memory_of<urd::repr::ParallelNodeHashMap<K, V>>
{
    static std::size_t bytes(const repr::ParallelNodeHashMap<K, V>& held) { return detail::sharded_table_bytes(held.table()); }
};

namespace urd::detail
{

template<typename S>
struct representation_of<parallel_hash_map_decl, S>
{
    using type = repr::ParallelHashMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<parallel_node_hash_map_decl, S>
{
    using type = repr::ParallelNodeHashMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
