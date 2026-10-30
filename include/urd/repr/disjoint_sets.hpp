#pragma once

#include "urd/elements.hpp"
#include "urd/repr/disjoint_sets_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A partition of the indices 0..n-1 (urd::index_of) into disjoint classes, each named by one of its elements:
/// union-find, by size and with path halving. Inserting a key makes it and every smaller index an element, each in a
/// class of its own; merge joins two classes. The registry names it for any key type, so it is an unconstrained
/// template; its container_traits declare properties only for indexable keys.
template<typename K>
class DisjointSets
{
public:
    using value_type = K;
    using key_type = K;
    using size_type = std::size_t;

    DisjointSets() = default;

    template<std::input_iterator InputIt>
    DisjointSets(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& element) { this->insert(K(element)); });
    }

    /// Makes `element`, and every smaller index not yet one, an element in a class of its own.
    void insert(const K& element)
    {
        const auto index = index_of<K>::to_index(element);
        if (index < m_parent.size())
            return;
        const auto old_size = m_parent.size();
        m_parent.resize(index + 1);
        std::iota(m_parent.begin() + static_cast<std::ptrdiff_t>(old_size), m_parent.end(), old_size);
        m_sizes.resize(index + 1, 1);
        m_classes += index + 1 - old_size;
    }

    [[nodiscard]] size_type size() const { return m_parent.size(); }
    /// Whether `element` is an element: its index is below size().
    [[nodiscard]] bool contains(const K& element) const { return index_of<K>::to_index(element) < m_parent.size(); }
    /// How many classes there are.
    [[nodiscard]] size_type classes() const { return m_classes; }

    /// The element that names the class of `element`; it changes only when the class is merged.
    [[nodiscard]] K find(const K& element) const { return index_of<K>::from_index(root(index_of<K>::to_index(element))); }
    [[nodiscard]] bool same(const K& lhs, const K& rhs) const { return root(index_of<K>::to_index(lhs)) == root(index_of<K>::to_index(rhs)); }
    /// Joins the classes of `lhs` and `rhs`; false where they were one already.
    bool merge(const K& lhs, const K& rhs)
    {
        auto first = root(index_of<K>::to_index(lhs));
        auto second = root(index_of<K>::to_index(rhs));
        if (first == second)
            return false;
        if (m_sizes[first] < m_sizes[second])
            std::swap(first, second);
        m_parent[second] = first;
        m_sizes[first] += m_sizes[second];
        --m_classes;
        return true;
    }
    /// How many elements share the class of `element`.
    [[nodiscard]] size_type class_size(const K& element) const { return m_sizes[root(index_of<K>::to_index(element))]; }

    void clear()
    {
        m_parent.clear();
        m_sizes.clear();
        m_classes = 0;
    }

private:
    /// The root of `index`'s tree, halving the path on the way: each node visited points to its grandparent.
    [[nodiscard]] std::size_t root(std::size_t index) const
    {
        while (m_parent[index] != index)
        {
            m_parent[index] = m_parent[m_parent[index]];
            index = m_parent[index];
        }
        return index;
    }

    // Path halving changes the trees, not the classes, so lookups stay const.
    mutable std::vector<std::size_t> m_parent;
    std::vector<std::size_t> m_sizes;
    std::size_t m_classes = 0;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd
