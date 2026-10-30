#pragma once

#include "urd/elements.hpp"
#include "urd/repr/entry_ordered_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A map whose entries are kept in Order (urd::EntryLess: by P(key, value), ties by key), which may read the values:
/// one vector of entries in that order, found by key by a scan. For few entries; every change of a value puts its
/// entry back in order, so values change only through update and insert_or_assign, never through a reference.
// ponytail: O(n) lookup and repositioning; a hash index beside an ordered one (Boost.MultiIndex) where maps grow large.
template<typename K, typename V, typename Order>
class FlatEntryMap
{
    using Entry = std::pair<K, V>;
    using Entries = std::vector<Entry>;

public:
    using iterator = typename Entries::const_iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    using value_type = Entry;
    using order_type = Order;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    FlatEntryMap() = default;

    template<std::input_iterator InputIt>
    FlatEntryMap(InputIt first, InputIt last)
    {
        insert(first, last);
    }

    [[nodiscard]] iterator begin() const { return m_entries.cbegin(); }
    [[nodiscard]] iterator end() const { return m_entries.cend(); }
    [[nodiscard]] size_type size() const { return m_entries.size(); }
    [[nodiscard]] bool empty() const { return m_entries.empty(); }
    void reserve(size_type count) { m_entries.reserve(count); }

    [[nodiscard]] iterator find(const K& key) const
    {
        return std::ranges::find_if(m_entries, [&key](const Entry& entry) { return EqualTo<K> {}(entry.first, key); });
    }
    [[nodiscard]] size_type count(const K& key) const { return contains(key) ? 1 : 0; }
    [[nodiscard]] bool contains(const K& key) const { return find(key) != end(); }
    [[nodiscard]] const V& at(const K& key) const
    {
        const auto found = find(key);
        if (found == end())
            throw std::out_of_range("urd::repr::FlatEntryMap::at: no value for the key");
        return found->second;
    }

    /// update(value, held) on the value of `key`, a default one where it has none (held false); then the entry goes
    /// back in order. What update returns.
    template<typename Update>
        requires std::invocable<Update&, V&, bool>
    decltype(auto) update(const K& key, Update update)
    {
        auto found = mutable_at(find(key));
        const bool held = found != m_entries.end();
        auto entry = held ? std::move(*found) : Entry(key, V());
        if (held)
            m_entries.erase(found);
        if constexpr (std::is_void_v<std::invoke_result_t<Update&, V&, bool>>)
        {
            update(entry.second, held);
            place(std::move(entry));
        }
        else
        {
            decltype(auto) result = update(entry.second, held);
            place(std::move(entry));
            return result;
        }
    }
    template<typename Read, typename Result>
        requires std::invocable<Read&, const V&>
    [[nodiscard]] Result read(const K& key, Read read, Result missing) const
    {
        const auto found = find(key);
        return found == end() ? missing : Result(read(found->second));
    }

    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
    {
        if (const auto found = find(key); found != end())
            return { found, false };
        return { place(Entry(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...))), true };
    }
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        const bool held = contains(key);
        update(key, [&mapped](V& value, bool) { value = std::forward<M>(mapped); });
        return { find(key), !held };
    }
    /// A key and its value, as any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    std::pair<iterator, bool> insert(const Pair& pair)
    {
        return try_emplace(K(pair.first), pair.second);
    }
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    {
        return insert(pair).first;
    }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::pair<K, V>(std::forward<Args>(args)...));
    }

    size_type erase(const K& key)
    {
        const auto found = find(key);
        if (found == end())
            return 0;
        erase(found);
        return 1;
    }
    iterator erase(const_iterator pos) { return m_entries.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_entries.erase(first, last); }
    void clear() { m_entries.clear(); }

    /// The same entries, in the same order, by urd::EqualTo.
    [[nodiscard]] friend bool operator==(const FlatEntryMap& lhs, const FlatEntryMap& rhs)
        requires EqualityComparableByEqualTo<V>
    {
        return std::ranges::equal(lhs.m_entries,
                                  rhs.m_entries,
                                  [](const Entry& left, const Entry& right)
                                  { return EqualTo<K> {}(left.first, right.first) && EqualTo<V> {}(left.second, right.second); });
    }

private:
    [[nodiscard]] typename Entries::iterator mutable_at(const_iterator pos) { return m_entries.begin() + (pos - m_entries.cbegin()); }
    /// Puts `entry` where Order says, after its equals (there are none: ties go by key).
    iterator place(Entry entry)
    {
        const auto at = std::ranges::upper_bound(m_entries, entry, Order {});
        return m_entries.insert(at, std::move(entry));
    }

    Entries m_entries;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd
