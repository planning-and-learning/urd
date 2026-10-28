#pragma once

#include "urd/elements.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A sorted map that keeps erased values for reuse: a std::map, and the nodes of erased keys, their values cleared but
/// holding their storage. A new key takes such a node, value and all, before allocating one: the buckets of a cost
/// queue, emptied and refilled, allocate nothing once warm.
template<typename K, typename V>
class RecyclingMap : public std::map<K, V, Less<K>>
{
    using Base = std::map<K, V, Less<K>>;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using typename Base::value_type;

    RecyclingMap() = default;

    template<std::input_iterator InputIt>
    RecyclingMap(InputIt first, InputIt last)
    { insert(first, last); }

    /// A value built from `args` for `key` unless it has one; a kept node first, where there is one.
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
    {
        if (const auto found = Base::find(key); found != Base::end())
            return { found, false };
        if (m_free.empty())
            return Base::try_emplace(key, std::forward<Args>(args)...);
        auto node = std::move(m_free.back());
        m_free.pop_back();
        node.key() = key;
        if constexpr (sizeof...(Args) > 0)
            node.mapped() = V(std::forward<Args>(args)...);
        return { Base::insert(std::move(node)).position, true };
    }
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const K& key, M&& mapped)
    {
        const auto [at, inserted] = try_emplace(key);
        at->second = std::forward<M>(mapped);
        return { at, inserted };
    }
    V& operator[](const K& key) { return try_emplace(key).first->second; }

    /// A key and its value, as any pair (std::pair<const K, V>, or what iterating another map gives).
    template<typename Pair>
        requires requires(const Pair& pair) {
            K(pair.first);
            V(pair.second);
        }
    std::pair<iterator, bool> insert(const Pair& pair)
    { return try_emplace(K(pair.first), pair.second); }
    template<typename Pair>
    iterator insert(const_iterator /*hint*/, const Pair& pair)
    { return insert(pair).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& pair) { this->insert(pair); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(std::pair<K, V>(std::forward<Args>(args)...)); }

    size_type erase(const K& key)
    {
        const auto found = Base::find(key);
        if (found == Base::end())
            return 0;
        erase(found);
        return 1;
    }
    /// Keeps the node: its value cleared (or made anew where it has no clear), its storage held.
    iterator erase(const_iterator pos)
    {
        const auto after = std::next(Base::erase(pos, pos));
        auto node = Base::extract(pos);
        if constexpr (requires(V& value) { value.clear(); })
            node.mapped().clear();
        else
            node.mapped() = V();
        m_free.push_back(std::move(node));
        return after;
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        while (first != last)
            first = erase(first);
        return Base::erase(first, first);
    }
    void clear()
    {
        while (!Base::empty())
            erase(Base::cbegin());
    }

private:
    std::vector<typename Base::node_type> m_free;
};

}  // namespace urd::repr

namespace urd
{

/// A tree whose erased nodes, and the values in them, are kept for the next keys.
template<orderable K, typename V>
struct container_traits<repr::RecyclingMap<K, V>> : properties_of<sorted, unique, sparse, stable_refs, shrinkable, retains, proportional_clear>
{
};

/// Its values keep what they hold when their keys are erased: their own properties, memory included.
template<typename K, typename V>
struct element_traits<repr::RecyclingMap<K, V>> : container_traits<V>
{
};

}  // namespace urd
