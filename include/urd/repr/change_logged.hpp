#pragma once

#include "urd/interface.hpp"
#include "urd/repr/change_logged_fwd.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <iterator>
#include <span>
#include <type_traits>
#include <utility>

namespace urd::repr
{

/// A set with unique elements that records what changed since its log was last cleared: the elements added that
/// were not there before, and those removed that were. An element added and removed again is in neither. Kept in R;
/// the log is two indexed sets, in the order the changes happened. The registry names it for any element type, so
/// it is an unconstrained template; its container_traits declare properties only where R's do.
template<typename R>
class ChangeLogged
{
public:
    using iterator = typename R::const_iterator;
    using const_iterator = iterator;
    using value_type = typename R::value_type;
    using key_type = typename R::value_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    ChangeLogged() = default;

    template<std::input_iterator InputIt>
    ChangeLogged(InputIt first, InputIt last)
    { insert(first, last); }

    [[nodiscard]] iterator begin() const { return m_elements.begin(); }
    [[nodiscard]] iterator end() const { return m_elements.end(); }
    [[nodiscard]] size_type size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }

    /// The elements added since the log was cleared, and not removed since.
    [[nodiscard]] std::span<const value_type> added() const { return { m_added.data(), m_added.size() }; }
    /// The elements there when the log was cleared, and removed since.
    [[nodiscard]] std::span<const value_type> removed() const { return { m_removed.data(), m_removed.size() }; }
    [[nodiscard]] bool was_added(const value_type& element) const { return m_added.contains(element); }
    [[nodiscard]] bool was_removed(const value_type& element) const { return m_removed.contains(element); }
    /// Starts a new log: the contents as they are become the reference.
    void clear_changes()
    {
        m_added.clear();
        m_removed.clear();
    }

    [[nodiscard]] iterator find(const value_type& element) const { return m_elements.find(element); }
    [[nodiscard]] size_type count(const value_type& element) const { return m_elements.count(element); }
    [[nodiscard]] bool contains(const value_type& element) const { return m_elements.contains(element); }
    // The bounds, where R is sorted.
    [[nodiscard]] iterator lower_bound(const value_type& element) const
        requires requires(const R& set) { set.lower_bound(element); }
    { return m_elements.lower_bound(element); }
    [[nodiscard]] iterator upper_bound(const value_type& element) const
        requires requires(const R& set) { set.upper_bound(element); }
    { return m_elements.upper_bound(element); }
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const value_type& element) const
        requires requires(const R& set) { set.equal_range(element); }
    { return m_elements.equal_range(element); }
    /// Equal contents, as R compares them; the logs do not count.
    [[nodiscard]] friend bool operator==(const ChangeLogged& lhs, const ChangeLogged& rhs) { return lhs.m_elements == rhs.m_elements; }

    std::pair<iterator, bool> insert(const value_type& element)
    {
        const auto [found, inserted] = m_elements.insert(element);
        if (inserted && m_removed.erase(element) == 0)
            m_added.insert(element);
        return { found, inserted };
    }
    iterator insert(const_iterator /*hint*/, const value_type& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& element) { this->insert(value_type(element)); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }

    size_type erase(const value_type& element)
    {
        if (!m_elements.contains(element))
            return 0;
        removing(element);
        return m_elements.erase(element);
    }
    iterator erase(const_iterator pos)
    {
        removing(*pos);
        return m_elements.erase(pos);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        for (auto remaining = std::distance(first, last); remaining > 0; --remaining)
            first = erase(first);
        return first;
    }
    /// Removes every element, each logged as removed (or no longer added).
    void clear()
    {
        std::ranges::for_each(m_elements, [this](const value_type& element) { removing(element); });
        m_elements.clear();
    }

private:
    // ponytail: erasing from an indexed set is O(n) in the log; fine while deltas stay small next to the set.
    void removing(const value_type& element)
    {
        if (m_added.erase(element) == 0)
            m_removed.insert(element);
    }

    R m_elements;
    IndexedSet<value_type> m_added;
    IndexedSet<value_type> m_removed;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd
