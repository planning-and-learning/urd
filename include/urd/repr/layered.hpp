#pragma once

#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <iterator>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace urd::repr
{

namespace detail
{

template<typename R>
struct mapped_of
{
    using type = char;  // a set's: no at() is offered
};
template<urd::detail::map_container R>
struct mapped_of<R>
{
    using type = typename R::mapped_type;
};

}  // namespace detail

/// A set or map with unique keys that answers lookups from its own elements, then from a parent's, which it does
/// not own and never changes: a layer over a repository built earlier. It holds, iterates and erases only its own
/// elements; inserting what the parent has inserts nothing and finds the parent's. In a map, a key's own value
/// shadows the parent's: insert_or_assign writes its own, and at() copies the parent's value up before handing it
/// out to change. The parent must outlive the layer and not change while layered over.
template<typename R>
class Layered
{
    static constexpr bool is_map = urd::detail::map_container<R>;
    using mapped = typename detail::mapped_of<R>::type;

public:
    using iterator = typename R::const_iterator;
    using const_iterator = iterator;
    using key_type = typename R::key_type;
    using value_type = typename R::value_type;
    using mapped_type = std::conditional_t<is_map, mapped, void>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    Layered() = default;

    template<std::input_iterator InputIt>
    Layered(InputIt first, InputIt last) : m_own(first, last)
    {
    }

    /// Answers from `parent` too, from now on; nullptr ends it.
    void layer_over(const Layered* parent) { m_parent = parent; }
    [[nodiscard]] const Layered* parent() const { return m_parent; }

    [[nodiscard]] iterator begin() const { return m_own.begin(); }
    [[nodiscard]] iterator end() const { return m_own.end(); }
    [[nodiscard]] size_type size() const { return m_own.size(); }
    [[nodiscard]] bool empty() const { return m_own.empty(); }

    /// The own element, else the parent's, else end().
    [[nodiscard]] iterator find(const key_type& key) const
    {
        const auto found = m_own.find(key);
        if (found != m_own.end() || m_parent == nullptr || !m_parent->contains(key))
            return found;
        return m_parent->find(key);
    }
    [[nodiscard]] bool contains(const key_type& key) const { return m_own.contains(key) || (m_parent != nullptr && m_parent->contains(key)); }
    [[nodiscard]] size_type count(const key_type& key) const { return contains(key) ? 1 : 0; }

    std::pair<iterator, bool> insert(const value_type& element)
    {
        if (const auto found = in_parent(element))
            return { *found, false };
        return m_own.insert(element);
    }
    std::pair<iterator, bool> insert(value_type&& element)
    {
        if (const auto found = in_parent(element))
            return { *found, false };
        return m_own.insert(std::move(element));
    }
    iterator insert(const_iterator /*hint*/, const value_type& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        for (; first != last; ++first)
            insert(value_type(*first));
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }

    /// Own elements only: the parent's stay.
    size_type erase(const key_type& key) { return m_own.erase(key); }
    iterator erase(const_iterator pos) { return m_own.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_own.erase(first, last); }
    void clear() { m_own.clear(); }

    [[nodiscard]] const mapped& at(const key_type& key) const
        requires is_map
    { return m_own.contains(key) || m_parent == nullptr ? m_own.at(key) : m_parent->at(key); }
    /// The own value, copied up from the parent first where only the parent has one.
    [[nodiscard]] mapped& at(const key_type& key)
        requires is_map
    {
        if (in_parent_only(key))
            m_own.try_emplace(key, m_parent->at(key));
        return m_own.at(key);
    }
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
        requires is_map
    {
        if (in_parent_only(key))
            return { m_parent->find(key), false };
        return m_own.try_emplace(key, std::forward<Args>(args)...);
    }
    template<typename M>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& value)
        requires is_map
    { return m_own.insert_or_assign(key, std::forward<M>(value)); }

private:
    /// The parent's element with the key of `element`, where only the parent has one.
    [[nodiscard]] std::optional<iterator> in_parent(const value_type& element) const
    {
        const auto& key = [&element]() -> const key_type&
        {
            if constexpr (is_map)
                return element.first;
            else
                return element;
        }();
        if (!in_parent_only(key))
            return std::nullopt;
        return m_parent->find(key);
    }
    [[nodiscard]] bool in_parent_only(const key_type& key) const { return m_parent != nullptr && !m_own.contains(key) && m_parent->contains(key); }

    R m_own;
    const Layered* m_parent = nullptr;
};

}  // namespace urd::repr

namespace urd
{

/// Its own elements as R holds them, plus the parent's in lookups.
template<typename R>
    requires has<container_traits<R>, unique>
struct container_traits<repr::Layered<R>> : with_t<container_traits<R>, layered>
{
};

}  // namespace urd
