#pragma once

#include "urd/props.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace urd
{

template<typename Container, props P>
class Refined;

template<typename Container, props P>
struct properties_of<Refined<Container, P>>
{
    static constexpr props value = P;
};

/// A container that owns its elements and can grow and shrink, unlike a view such as std::span.
template<typename Container>
concept owning_sequence = std::ranges::random_access_range<Container>
    && requires(Container& container, const typename Container::value_type& value) {
           container.insert(container.begin(), value);
           container.erase(container.begin(), container.end());
       };

/// A container whose contents always have the properties P.
///
/// P is established once, by make() (which does the work) or assume() (which takes the caller's
/// word and checks it in Debug builds), and every member keeps it: elements are read-only, and the
/// only changes are those that preserve P. Converting to fewer properties is implicit, because it
/// is always safe; gaining properties goes through make() or assume() again.
template<typename Container, props P>
class Refined
{
public:
    using container_type = Container;
    using value_type = typename Container::value_type;
    using const_iterator = std::ranges::iterator_t<const Container>;
    static constexpr props properties = P;

    /// Sorts and removes duplicates as P asks; duplicates keep their first occurrence.
    [[nodiscard]] static Refined make(Container container)
        requires owning_sequence<Container> && (!includes(P, props::nonempty))
    {
        if constexpr (includes(P, props::sorted))
            std::ranges::sort(container);
        if constexpr (includes(P, props::unique))
            remove_duplicates(container);
        return Refined(std::move(container));
    }

    /// Takes `container` as having P; Debug builds check it.
    [[nodiscard]] static Refined assume(Container container)
    {
        assert(holds<P>(container) && "urd::Refined::assume: the contents lack a claimed property");
        return Refined(std::move(container));
    }

    Refined()
        requires(!includes(P, props::nonempty))
    = default;

    [[nodiscard]] const_iterator begin() const { return m_container.begin(); }
    [[nodiscard]] const_iterator end() const { return m_container.end(); }
    [[nodiscard]] std::size_t size() const { return std::ranges::size(m_container); }
    [[nodiscard]] bool empty() const { return std::ranges::empty(m_container); }
    [[nodiscard]] const value_type& operator[](std::size_t pos) const { return m_container[pos]; }
    [[nodiscard]] const Container& get() const& { return m_container; }

    /// The first element; only a nonempty range has one, so no check is needed.
    [[nodiscard]] const value_type& front() const
        requires(includes(P, props::nonempty))
    {
        return *m_container.begin();
    }

    /// Inserts `value` where P allows: at its sorted position, else at the end. With `unique`, an
    /// element equal to `value` already present leaves the contents unchanged.
    /// Returns whether `value` was inserted.
    bool insert(const value_type& value)
        requires owning_sequence<Container>
    {
        if constexpr (includes(P, props::sorted))
        {
            const auto pos = std::ranges::lower_bound(m_container, value);
            if (includes(P, props::unique) && pos != m_container.end() && *pos == value)
                return false;
            m_container.insert(pos, value);
        }
        else
        {
            if (includes(P, props::unique) && std::ranges::find(m_container, value) != m_container.end())
                return false;
            m_container.insert(m_container.end(), value);
        }
        return true;
    }

    /// Removes every element equal to `value`; returns how many. A nonempty range cannot offer
    /// this, since it could remove the last element.
    std::size_t erase(const value_type& value)
        requires owning_sequence<Container> && (!includes(P, props::nonempty))
    {
        if constexpr (includes(P, props::sorted))
        {
            const auto [first, last] = std::ranges::equal_range(m_container, value);
            const auto count = static_cast<std::size_t>(last - first);
            m_container.erase(first, last);
            return count;
        }
        else
        {
            const auto removed = std::ranges::remove(m_container, value);
            const auto count = std::ranges::size(removed);
            m_container.erase(removed.begin(), removed.end());
            return count;
        }
    }

    /// A read-only view of the contents, with the same properties.
    [[nodiscard]] Refined<std::span<const value_type>, P> view() const
        requires std::ranges::contiguous_range<Container>
    {
        return Refined<std::span<const value_type>, P>(std::span<const value_type>(m_container));
    }

    /// Drops properties; always safe, so implicit.
    template<props Q>
        requires(includes(P, Q) && Q != P)
    operator Refined<Container, Q>() const&
    {
        return Refined<Container, Q>(m_container);
    }

    /// Gives up every property, visibly, and returns the contents.
    [[nodiscard]] Container release() && { return std::move(m_container); }

    [[nodiscard]] friend bool operator==(const Refined& lhs, const Refined& rhs)
        requires std::equality_comparable<Container>
    {
        return lhs.m_container == rhs.m_container;
    }

private:
    template<typename, props>
    friend class Refined;

    explicit Refined(Container container) : m_container(std::move(container)) {}

    /// Keeps the first of equal elements, in their order; O(n log n), or O(n) when sorted.
    static void remove_duplicates(Container& container)
    {
        if constexpr (includes(P, props::sorted))
        {
            const auto duplicates = std::ranges::unique(container);
            container.erase(duplicates.begin(), duplicates.end());
        }
        else
        {
            auto order = std::vector<std::size_t>(container.size());
            std::iota(order.begin(), order.end(), std::size_t { 0 });
            std::ranges::stable_sort(order, {}, [&container](std::size_t pos) -> const value_type& { return container[pos]; });
            auto keep = std::vector<bool>(container.size(), true);
            for (std::size_t k = 1; k < order.size(); ++k)
                if (container[order[k]] == container[order[k - 1]])
                    keep[order[k]] = false;
            std::size_t out = 0;
            for (std::size_t pos = 0; pos < container.size(); ++pos)
                if (keep[pos])
                    container[out++] = std::move(container[pos]);
            container.erase(container.begin() + static_cast<std::ptrdiff_t>(out), container.end());
        }
    }

    Container m_container;
};

template<typename T>
using SortedVector = Refined<std::vector<T>, props::sorted>;

template<typename T>
using UniqueVector = Refined<std::vector<T>, props::unique>;

template<typename T>
using SortedUniqueVector = Refined<std::vector<T>, props::sorted | props::unique>;

}  // namespace urd
