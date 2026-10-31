#pragma once

#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/scoped_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A map with scopes, for symbol tables: pop_scope undoes every change since the matching push_scope, so a binding
/// made in a scope shadows an outer one until the scope ends. The bindings in force are Map (a flat hash table, a
/// tree, a dense map); each change inside a scope first logs the key's previous value (or its absence). The
/// registry names it for any key type, so it is an unconstrained template; its container_traits declare properties
/// where Map's do.
template<typename Map>
class Scoped
{
    using Table = Map;
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

public:
    using iterator = typename Table::const_iterator;
    using const_iterator = iterator;
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    Scoped() = default;

    template<std::input_iterator InputIt>
    Scoped(InputIt first, InputIt last) : m_table(first, last)
    {
    }

    [[nodiscard]] iterator begin() const { return m_table.begin(); }
    [[nodiscard]] iterator end() const { return m_table.end(); }
    [[nodiscard]] size_type size() const { return m_table.size(); }
    [[nodiscard]] bool empty() const { return m_table.empty(); }
    /// Equal bindings in force: the table decides, whatever order it iterates in and whatever scopes are open.
    [[nodiscard]] friend bool operator==(const Scoped& lhs, const Scoped& rhs)
        requires std::equality_comparable<Table>
    { return lhs.m_table == rhs.m_table; }

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
    /// Whether `key` is bound and was bound in the innermost open scope (any scope being open); with none open,
    /// whether it is bound at all. A redeclaration check: the log since the last push_scope names the keys changed.
    [[nodiscard]] bool bound_in_scope(const K& key) const
    {
        if (!contains(key))
            return false;
        if (m_scopes.empty())
            return true;
        return std::any_of(m_log.begin() + static_cast<std::ptrdiff_t>(m_scopes.back()),
                           m_log.end(),
                           [&key](const auto& entry) { return EqualTo<K> {}(entry.first, key); });
    }

    [[nodiscard]] iterator find(const K& key) const { return m_table.find(key); }
    [[nodiscard]] size_type count(const K& key) const { return m_table.count(key); }
    [[nodiscard]] bool contains(const K& key) const { return m_table.contains(key); }
    // The bounds, where Map is sorted.
    [[nodiscard]] iterator lower_bound(const K& key) const
        requires requires(const Table& table) { table.lower_bound(key); }
    { return m_table.lower_bound(key); }
    [[nodiscard]] iterator upper_bound(const K& key) const
        requires requires(const Table& table) { table.upper_bound(key); }
    { return m_table.upper_bound(key); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) const
        requires requires(const Table& table) { table.equal_range(key); }
    { return m_table.equal_range(key); }
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

template<>
struct include<"urd/repr/scoped.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<scoped_map_decl, S>
{
    using type = repr::ScopedMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<scoped_tree_map_decl, S>
{
    using type = repr::ScopedTreeMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<scoped_dense_map_decl, S>
{
    using type = repr::ScopedDenseMap<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
