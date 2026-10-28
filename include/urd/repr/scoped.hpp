#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A map with scopes, for symbol tables: pop_scope undoes every change since the matching push_scope, so a binding
/// made in a scope shadows an outer one until the scope ends. The bindings in force are a flat hash table; each
/// change inside a scope first logs the key's previous value (or its absence). The registry names it for any key
/// type, so it is an unconstrained template; its container_traits declare properties only for hashable keys.
template<typename K, typename V>
class ScopedMap
{
    using Table = boost::unordered_flat_map<K, V, Hash<K>, EqualTo<K>>;

public:
    using iterator = typename Table::const_iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    ScopedMap() = default;

    template<std::input_iterator InputIt>
    ScopedMap(InputIt first, InputIt last) : m_table(first, last)
    {
    }

    [[nodiscard]] iterator begin() const { return m_table.begin(); }
    [[nodiscard]] iterator end() const { return m_table.end(); }
    [[nodiscard]] size_type size() const { return m_table.size(); }
    [[nodiscard]] bool empty() const { return m_table.empty(); }

    /// Opens a scope: the changes from here on are undone by the matching pop_scope.
    void push_scope() { m_scopes.push_back(m_log.size()); }
    /// Undoes every change since the matching push_scope, latest first.
    void pop_scope()
    {
        assert(!m_scopes.empty() && "urd::repr::ScopedMap::pop_scope: no scope is open");
        for (; m_log.size() > m_scopes.back(); m_log.pop_back())
        {
            auto& [key, previous] = m_log.back();
            if (previous)
                m_table.insert_or_assign(key, std::move(*previous));
            else
                m_table.erase(key);
        }
        m_scopes.pop_back();
    }
    /// How many scopes are open.
    [[nodiscard]] size_type depth() const { return m_scopes.size(); }

    [[nodiscard]] iterator find(const K& key) const { return m_table.find(key); }
    [[nodiscard]] size_type count(const K& key) const { return m_table.count(key); }
    [[nodiscard]] bool contains(const K& key) const { return m_table.contains(key); }
    [[nodiscard]] const V& at(const K& key) const { return m_table.at(key); }
    /// The value, to change in place: logged first, as any change.
    [[nodiscard]] V& at(const K& key)
    {
        auto& value = m_table.at(key);
        log(key);
        return value;
    }

    std::pair<iterator, bool> insert(const value_type& pair) { return try_emplace(pair.first, pair.second); }
    iterator insert(const_iterator /*hint*/, const value_type& pair) { return insert(pair).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(value_type(pair)); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
    {
        if (!m_table.contains(key))
            log(key);
        return m_table.try_emplace(key, std::forward<Args>(args)...);
    }
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        log(key);
        return m_table.insert_or_assign(key, std::forward<M>(mapped));
    }

    size_type erase(const K& key)
    {
        if (!m_table.contains(key))
            return 0;
        log(key);
        return m_table.erase(key);
    }
    iterator erase(const_iterator pos)
    {
        log(pos->first);
        return m_table.erase(pos);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        for (auto remaining = std::distance(first, last); remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    /// Removes every binding and closes every scope.
    void clear()
    {
        m_table.clear();
        m_log.clear();
        m_scopes.clear();
    }

private:
    /// Records the key's value before a change, where a scope is open to undo it.
    void log(const K& key)
    {
        if (m_scopes.empty())
            return;
        const auto found = m_table.find(key);
        m_log.emplace_back(key, found == m_table.end() ? std::nullopt : std::optional<V>(found->second));
    }

    Table m_table;
    std::vector<std::pair<K, std::optional<V>>> m_log;
    std::vector<std::size_t> m_scopes;
};

}  // namespace urd::repr

namespace urd
{

/// Bindings in a flat hash table, with scopes undone by a log; erase and clear keep the table's slots.
template<hashable K, typename V>
struct container_traits<repr::ScopedMap<K, V>> : properties_of<unique, sparse, shrinkable, scoped>
{
};

}  // namespace urd
