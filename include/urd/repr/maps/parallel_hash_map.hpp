#pragma once

// gtl's sharded hash maps (flat, or a node per element), defined. Include it where a Store resolves to one.

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

namespace urd
{

template<>
struct include<"urd/repr/maps/parallel_hash_map.hpp">
{
};

}  // namespace urd

namespace urd::repr
{

/// A gtl sharded table whose update and read run under the lock of the key's shard: safe while other threads do the
/// same.
template<typename Table>
class LockedByKey : public Table
{
public:
    using Table::Table;
    using typename Table::key_type;
    using typename Table::mapped_type;

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
    /// read(value) where `key` has a value, else `missing`, under the key's lock.
    template<typename Read, typename Result>
    [[nodiscard]] Result read(const key_type& key, Read& read, Result missing) const
    {
        auto result = std::move(missing);
        this->if_contains(key, [&read, &result](const auto& entry) { result = Result(read(entry.second)); });
        return result;
    }
};

template<typename K, typename V>
class ParallelHashMap :
    public LockedByKey<gtl::parallel_flat_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>>
{
public:
    using ParallelHashMap::LockedByKey::LockedByKey;
};

template<typename K, typename V>
class ParallelNodeHashMap :
    public LockedByKey<gtl::parallel_node_hash_map<K, V, Hash<K>, EqualTo<K>, std::allocator<std::pair<const K, V>>, shard_bits, std::mutex>>
{
public:
    using ParallelNodeHashMap::LockedByKey::LockedByKey;
};

}  // namespace urd::repr

template<typename K, typename V>
struct urd::memory_of<urd::repr::ParallelHashMap<K, V>>
{
    static std::size_t bytes(const repr::ParallelHashMap<K, V>& held) { return detail::sharded_table_bytes(held); }
};
template<typename K, typename V>
struct urd::memory_of<urd::repr::ParallelNodeHashMap<K, V>>
{
    static std::size_t bytes(const repr::ParallelNodeHashMap<K, V>& held) { return detail::sharded_table_bytes(held); }
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
