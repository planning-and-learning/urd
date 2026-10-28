#pragma once

#include "urd/elements.hpp"

#include <cstddef>
#include <numeric>
#include <utility>
#include <vector>

namespace urd
{

/// A partition of the indices 0..n-1 (urd::index_of) into disjoint classes, each named by one of its elements:
/// union-find, by size and with path halving. Every element starts in a class of its own; merge joins two classes.
/// Not a Store kind of its own: its elements are the indices, and what it answers is which class each is in.
template<typename K>
class Partition
{
    static_assert(indexable<K>, "urd::Partition: elements must be indices (urd::index_of)");

public:
    Partition() = default;
    /// The elements 0..size-1, each in a class of its own.
    explicit Partition(std::size_t size) : m_parent(size), m_sizes(size, 1), m_classes(size) { std::iota(m_parent.begin(), m_parent.end(), std::size_t { 0 }); }

    [[nodiscard]] std::size_t size() const { return m_parent.size(); }
    /// How many classes there are.
    [[nodiscard]] std::size_t classes() const { return m_classes; }

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
    [[nodiscard]] std::size_t class_size(const K& element) const { return m_sizes[root(index_of<K>::to_index(element))]; }

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

}  // namespace urd
