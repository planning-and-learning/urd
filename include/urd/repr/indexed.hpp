#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A set kept in insertion order: the elements in a vector, each with its position in a hash table. An element's
/// position is its index, which never changes while nothing is erased: an interner. The registry names it for
/// any element type, so it is an unconstrained template; its container_traits declare properties only for
/// hashable elements.
template<typename T>
class IndexedSet
{
public:
    using iterator = typename std::vector<T>::const_iterator;
    using const_iterator = iterator;
    using value_type = T;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndexedSet() = default;

    template<std::input_iterator InputIt>
    IndexedSet(InputIt first, InputIt last)
    { insert(first, last); }

    [[nodiscard]] iterator begin() const { return m_elements.begin(); }
    [[nodiscard]] iterator end() const { return m_elements.end(); }
    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }
    [[nodiscard]] const T* data() const { return m_elements.data(); }

    void reserve(size_type count)
    {
        m_elements.reserve(count);
        m_positions.reserve(count);
    }

    [[nodiscard]] iterator find(const T& element) const
    {
        const auto found = m_positions.find(element);
        return found == m_positions.end() ? end() : begin() + static_cast<difference_type>(found->second);
    }
    [[nodiscard]] size_type count(const T& element) const { return m_positions.count(element); }
    [[nodiscard]] bool contains(const T& element) const { return m_positions.contains(element); }

    std::pair<iterator, bool> insert(const T& element)
    {
        const auto [found, inserted] = m_positions.try_emplace(element, m_elements.size());
        if (inserted)
            m_elements.push_back(element);
        return { begin() + static_cast<difference_type>(found->second), inserted };
    }
    std::pair<iterator, bool> insert(T&& element)
    {
        const auto [found, inserted] = m_positions.try_emplace(element, m_elements.size());
        if (inserted)
            m_elements.push_back(std::move(element));
        return { begin() + static_cast<difference_type>(found->second), inserted };
    }
    /// New elements go last, wherever the hint is.
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
        const auto found = m_positions.find(element);
        if (found == m_positions.end())
            return 0;
        erase(begin() + static_cast<difference_type>(found->second));
        return 1;
    }
    iterator erase(const_iterator pos) { return erase(pos, std::next(pos)); }
    /// Later elements move up to keep the order, so their positions change.
    // ponytail: O(n) per erase, reindexing what follows; an interner never erases.
    iterator erase(const_iterator first, const_iterator last)
    {
        const auto from = static_cast<size_type>(first - begin());
        std::for_each(first, last, [this](const T& element) { m_positions.erase(element); });
        m_elements.erase(first, last);
        for (auto place = from; place < m_elements.size(); ++place)
            m_positions[m_elements[place]] = place;
        return begin() + static_cast<difference_type>(from);
    }
    void clear()
    {
        m_elements.clear();
        m_positions.clear();
    }

    /// Equal elements in the same order.
    [[nodiscard]] friend bool operator==(const IndexedSet& lhs, const IndexedSet& rhs) { return lhs.m_elements == rhs.m_elements; }

private:
    std::vector<T> m_elements;
    boost::unordered_flat_map<T, size_type, Hash<T>, EqualTo<T>> m_positions;
};

}  // namespace urd::repr

namespace urd
{

/// In insertion order, in one array; erase and clear keep the storage of both the array and the table.
template<hashable T>
struct container_traits<repr::IndexedSet<T>> : properties_of<insertion_ordered, unique, contiguous, shrinkable, retains>
{
};

}  // namespace urd
