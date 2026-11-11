#pragma once

#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/core/std_memory.hpp"
#include "urd/repr/maps/std_map.hpp"
#include "urd/repr/values/std_set.hpp"
#include "urd/repr/wrappers/recycling_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace urd::repr
{

namespace detail
{

/// What a recycling map and set share: Container (a std map or set, whose nodes can be taken out and put back), the
/// nodes of its erased elements, and erasing into them.
template<typename Container>
class Recycled : public Container
{
public:
    using typename Container::const_iterator;
    using typename Container::iterator;
    using typename Container::key_type;
    using typename Container::size_type;

    size_type erase(const key_type& key)
    {
        const auto found = Container::find(key);
        if (found == Container::end())
            return 0;
        erase(const_iterator(found));
        return 1;
    }
    /// Keeps the node, its storage held; a map's value cleared (or made anew where it has no clear).
    iterator erase(const_iterator pos)
    {
        const auto after = std::next(Container::erase(pos, pos));
        auto node = Container::extract(pos);
        reset(node);
        m_free.push_back(std::move(node));
        return after;
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        while (first != last)
            first = erase(first);
        return Container::erase(first, first);
    }
    void clear()
    {
        while (!Container::empty())
            erase(Container::cbegin());
    }

protected:
    template<typename Node>
        requires requires(Node& node) { node.mapped(); }
    static void reset(Node& node)
    {
        if constexpr (requires { node.mapped().clear(); })
            node.mapped().clear();
        else
            node.mapped() = typename Node::mapped_type();
    }
    template<typename Node>
    static void reset(Node& /*a set's node: nothing to clear*/)
    {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): the derived map and set take their nodes from it
    std::vector<typename Container::node_type> m_free;
};

}  // namespace detail

/// A map that keeps erased values for reuse: Map (a std::map or std::unordered_map), and the nodes of erased keys,
/// their values cleared but holding their storage. A new key takes such a node, value and all, before allocating
/// one: values emptied and refilled under changing keys allocate nothing once warm.
template<typename Map>
class Recycling : public detail::Recycled<Map>
{
    using Base = detail::Recycled<Map>;
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;

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

private:
    friend struct urd::memory_of<Recycling>;

    using Base::m_free;
};

/// A set that keeps erased nodes for reuse: Set (a std::set or std::unordered_set), and the nodes of erased elements,
/// holding their storage. A new element takes such a node before allocating one, so a set emptied and refilled
/// allocates nothing once warm, and references to its elements hold as the set's do.
template<typename Set>
class RecyclingSet : public detail::Recycled<Set>
{
    using Base = detail::Recycled<Set>;
    using T = typename Set::value_type;

public:
    using typename Base::const_iterator;
    using typename Base::iterator;

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

    friend struct urd::memory_of<RecyclingSet>;

    using Base::m_free;
};

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// The kept nodes: the table of them, each node, and what its value still owns.
template<typename Free>
std::size_t kept_bytes(const Free& free)
{
    using Handle = typename Free::value_type;
    auto result = (free.capacity() * sizeof(Handle)) + (free.size() * kept_node<Handle>::bytes);
    for (const auto& node : free)
    {
        if constexpr (requires { node.mapped(); })
            result += memory_usage(node.key()) + memory_usage(node.mapped());
        else
            result += memory_usage(node.value());
    }
    return result;
}

}  // namespace detail

/// Its map's, and the nodes it keeps.
template<typename Map>
struct memory_of<repr::Recycling<Map>>
{
    static std::size_t bytes(const repr::Recycling<Map>& held) { return memory_usage(static_cast<const Map&>(held)) + detail::kept_bytes(held.m_free); }
};
/// Its set's, and the nodes it keeps.
template<typename Set>
struct memory_of<repr::RecyclingSet<Set>>
{
    static std::size_t bytes(const repr::RecyclingSet<Set>& held) { return memory_usage(static_cast<const Set&>(held)) + detail::kept_bytes(held.m_free); }
};

template<>
struct include<"urd/repr/wrappers/recycling.hpp">
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
    using type = repr::Recycling<std::map<typename S::key_type, typename S::mapped_type, typename S::order_type>>;
};

}  // namespace urd::detail
