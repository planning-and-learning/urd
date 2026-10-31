#pragma once

// Postings: a multimap whose values lie together per key in a flat hash table. The grouped multimap over hashed
// groups; the one grouped container that pulls in Boost.Unordered.

#include "urd/elements.hpp"
#include "urd/registry/row.hpp"
#include "urd/repr/grouped_fwd.hpp"
#include "urd/repr/grouped_multimap.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstddef>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

namespace urd::repr
{

/// Groups as postings: a flat hash table from each key to the vector of its values, in the order inserted, so looking
/// a key's values up is one probe; a group is an entry of the table. A key whose last value is erased leaves it.
template<typename K, typename V>
class HashGroups
{
    using Table = boost::unordered_flat_map<K, std::vector<V>, Hash<K>, EqualTo<K>>;

public:
    using key_type = K;
    using mapped_type = V;
    using group = typename Table::const_iterator;
    static constexpr bool ordered = false;
    static constexpr bool changes = true;

    HashGroups() = default;

    template<std::input_iterator InputIt>
    HashGroups(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->append(K(pair.first), V(pair.second)); });
    }

    [[nodiscard]] group begin() const { return m_table.begin(); }
    [[nodiscard]] group end() const { return m_table.end(); }
    [[nodiscard]] std::size_t count() const { return m_count; }
    [[nodiscard]] const K& key(group at) const { return at->first; }
    [[nodiscard]] std::span<const V> row(group at) const { return at->second; }
    [[nodiscard]] group find(const K& key) const { return m_table.find(key); }

    /// The group of `key` and the position of the value appended to it.
    std::pair<group, std::size_t> append(const K& key, V value)
    {
        const auto [found, inserted] = m_table.try_emplace(key);
        found->second.push_back(std::move(value));
        ++m_count;
        return { found, found->second.size() - 1 };
    }
    std::size_t erase(const K& key)
    {
        const auto found = m_table.find(key);
        if (found == m_table.end())
            return 0;
        const auto erased = found->second.size();
        m_table.erase(found);
        m_count -= erased;
        return erased;
    }
    /// Where the value after the erased one is.
    std::pair<group, std::size_t> erase(group at, std::size_t index)
    {
        auto& values = m_table.find(at->first)->second;
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
        --m_count;
        if (!values.empty())
            return { at, index };
        // Erasing from Boost's open-addressing table moves no other entry, so the next one stays where it is.
        const auto next = std::next(at);
        m_table.erase(at);
        return { next, 0 };
    }
    void clear()
    {
        m_table.clear();
        m_count = 0;
    }

private:
    Table m_table;
    std::size_t m_count = 0;
};

}  // namespace urd::repr

namespace urd
{

template<>
struct include<"urd/repr/postings.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"posting_map", S>
{
    using type = repr::PostingMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
