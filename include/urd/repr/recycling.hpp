#pragma once

#include "urd/elements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/recycling_fwd.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A map that keeps erased values for reuse: Map (a std::map or std::unordered_map, whose nodes can be taken out and
/// put back), and the nodes of erased keys, their values cleared but holding their storage. A new key takes such a
/// node, value and all, before allocating one: values emptied and refilled under changing keys allocate nothing once
/// warm.
template<typename Map>
class Recycling : public Map
{
    using Base = Map;
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using typename Base::value_type;

    Recycling() = default;

    template<std::input_iterator InputIt>
    Recycling(InputIt first, InputIt last)
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

/// A set that keeps erased nodes for reuse: Set (a std::set or std::unordered_set), and the nodes of erased elements,
/// holding their storage. A new element takes such a node before allocating one, so a set emptied and refilled
/// allocates nothing once warm, and references to its elements hold as the set's do.
template<typename Set>
class RecyclingSet : public Set
{
    using Base = Set;
    using T = typename Set::value_type;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::size_type;
    using typename Base::value_type;

    RecyclingSet() = default;

    template<std::input_iterator InputIt>
    RecyclingSet(InputIt first, InputIt last)
    { insert(first, last); }

    std::pair<iterator, bool> insert(const T& element) { return put(element); }
    std::pair<iterator, bool> insert(T&& element) { return put(std::move(element)); }
    iterator insert(const_iterator /*hint*/, const T& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& element) { this->insert(T(element)); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }

    size_type erase(const T& element)
    {
        const auto found = Base::find(element);
        if (found == Base::end())
            return 0;
        erase(found);
        return 1;
    }
    /// Keeps the node, its storage held.
    iterator erase(const_iterator pos)
    {
        const auto after = std::next(Base::erase(pos, pos));
        m_free.push_back(Base::extract(pos));
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
    /// Inserts a kept node holding `element` where there is one, else a new one.
    template<typename U>
    std::pair<iterator, bool> put(U&& element)
    {
        if (const auto found = Base::find(element); found != Base::end())
            return { found, false };
        if (m_free.empty())
            return Base::insert(std::forward<U>(element));
        auto node = std::move(m_free.back());
        m_free.pop_back();
        node.value() = std::forward<U>(element);
        return { Base::insert(std::move(node)).position, true };
    }

    std::vector<typename Base::node_type> m_free;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/recycling.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<recycling_hash_set_decl, S>
{
    using type = repr::RecyclingHashSet<typename S::element_type>;
};
template<typename S>
struct representation_of<recycling_tree_set_decl, S>
{
    using type = repr::RecyclingTreeSet<typename S::element_type>;
};
template<typename S>
struct representation_of<recycling_hash_map_decl, S>
{
    using type = repr::RecyclingHashMap<typename S::key_type, typename S::mapped_type>;
};
template<typename S>
struct representation_of<recycling_map_decl, S>
{
    using type = repr::Recycling<std::map<typename S::key_type, typename S::mapped_type, key_order_t<S>>>;
};

}  // namespace urd::detail
