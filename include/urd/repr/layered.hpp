#pragma once

#include "urd/index.hpp"
#include "urd/interface.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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
/// shadows the parent's: insert_or_assign writes its own, and at() makes an own value before handing it out to
/// change, a layer over the parent's where values layer (a map of layered pools layers through both levels), else
/// a copy of it. The parent must outlive the layer and not change while layered over.
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
    {
        return insert(value_type(std::forward<Args>(args)...));
    }

    /// Own elements only: the parent's stay.
    size_type erase(const key_type& key) { return m_own.erase(key); }
    iterator erase(const_iterator pos) { return m_own.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_own.erase(first, last); }
    void clear() { m_own.clear(); }

    [[nodiscard]] const mapped& at(const key_type& key) const
        requires is_map
    {
        return m_own.contains(key) || m_parent == nullptr ? m_own.at(key) : m_parent->at(key);
    }
    /// The own value: where only the parent has one, a value that layers (a layered Store) is made here as a layer
    /// over the parent's, else the parent's is copied up first.
    [[nodiscard]] mapped& at(const key_type& key)
        requires is_map
    {
        if (in_parent_only(key))
        {
            if constexpr (requires(mapped& own, const mapped* above) { own.layer_over(above); })
            {
                m_own.try_emplace(key);
                m_own.at(key).layer_over(&m_parent->at(key));
            }
            else
                m_own.try_emplace(key, m_parent->at(key));
        }
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
    {
        return m_own.insert_or_assign(key, std::forward<M>(value));
    }

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

namespace detail
{

template<typename R>
struct write_of
{
    using type = void;
};
template<typename R>
    requires requires { typename R::write_type; }
struct write_of<R>
{
    using type = typename R::write_type;
};

}  // namespace detail

/// A pool over a parent pool it neither owns nor changes: a layer. Indices continue after the parent's size at the
/// branch, so an index means the same in every layer below it; a lookup answers from this layer, then from the
/// parent's chain; an insert finds its record anywhere in the chain before adding it here. The parent must outlive
/// the layer and not grow after the branch (asserted). Over a pool of records (handles, unique, flat) or of rows.
template<typename R>
class LayeredPool
{
    using T = typename R::value_type;
    /// Row pools take writes to rows; record pools take records.
    static constexpr bool row_pool = requires { typename R::write_type; };
    /// A row's element; a record pool has none, and its row members are never instantiated.
    using element = std::ranges::range_value_t<std::conditional_t<row_pool, T, std::vector<std::monostate>>>;

public:
    using value_type = T;
    using handle_type = urd::index<T>;
    using write_type = typename detail::write_of<R>::type;
    using size_type = std::size_t;

    LayeredPool() = default;

    /// Answers from `parent` too, from now on, with indices continuing after its size; nullptr makes it a root. Only
    /// while this layer holds nothing of its own.
    void layer_over(const LayeredPool* parent)
    {
        assert(m_own.size() == 0 && "urd::repr::LayeredPool: a layer branches before it holds anything");
        m_parent = parent;
        m_base = parent == nullptr ? 0 : static_cast<std::uint32_t>(parent->size());
        m_layer = parent == nullptr ? 0 : parent->m_layer + 1;
    }
    [[nodiscard]] const LayeredPool* parent() const noexcept { return m_parent; }
    /// Where this layer's own indices start: the parent's size at the branch.
    [[nodiscard]] size_type base() const noexcept { return m_base; }
    /// This layer's number along the chain: 0 for a root.
    [[nodiscard]] std::size_t layer() const noexcept { return m_layer; }
    /// Whether the parent grew after the branch, which it must not.
    [[nodiscard]] bool parent_grew() const noexcept { return m_parent != nullptr && m_parent->size() > m_base; }

    [[nodiscard]] size_type size() const noexcept { return m_base + m_own.size(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // Records.

    std::pair<handle_type, bool> insert(const T& record)
        requires(!row_pool)
    {
        return insert(record, [&record](handle_type) -> const T& { return record; });
    }
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
        requires(!row_pool)
    {
        if (const auto at = find(key); !at.is_none())
            return { at, false };
        assert(!parent_grew() && "urd::repr::LayeredPool: the parent grew after the branch");
        const auto [local_at, inserted] = m_own.insert(key, [this, &make](handle_type at) { return make(global(at)); });
        return { global(local_at), inserted };
    }
    [[nodiscard]] handle_type find(const T& record) const
        requires(!row_pool)
    {
        if (const auto at = m_own.find(record); !at.is_none())
            return global(at);
        return m_parent == nullptr ? handle_type() : m_parent->find(record);
    }
    [[nodiscard]] bool contains(const T& record) const
        requires(!row_pool)
    {
        return !find(record).is_none();
    }
    [[nodiscard]] const T& operator[](handle_type at) const
        requires(!row_pool)
    {
        const auto& layer = owner(at);
        return layer.m_own[layer.local(at)];
    }

    // Rows.

    template<std::ranges::input_range Elements>
    handle_type insert(const Elements& elements)
        requires row_pool
    {
        if (const auto at = find(elements); !at.is_none())
            return at;
        assert(!parent_grew() && "urd::repr::LayeredPool: the parent grew after the branch");
        return global(m_own.insert(elements));
    }
    /// The row `base` with `writes` applied, stored here where new.
    handle_type insert(handle_type base, std::span<const write_type> writes)
        requires row_pool && requires(R& own, handle_type at, std::span<const write_type> some) { own.insert(at, some); }
    {
        auto row = elements_of(base);
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }
    /// The row `base` with `added` put in and `removed` taken out, stored here where new.
    handle_type insert(handle_type base, std::span<const element> added, std::span<const element> removed)
        requires row_pool
    {
        auto row = elements_of(base);
        row.insert(row.end(), added.begin(), added.end());
        std::erase_if(row, [&removed](const element& each) { return std::ranges::find(removed, each) != removed.end(); });
        return insert(row);
    }
    template<std::ranges::input_range Elements>
    [[nodiscard]] handle_type find(const Elements& elements) const
        requires row_pool
    {
        if (const auto at = m_own.find(elements); !at.is_none())
            return global(at);
        return m_parent == nullptr ? handle_type() : m_parent->find(elements);
    }
    [[nodiscard]] bool contains(handle_type at, const element& value) const
        requires row_pool
    {
        const auto& layer = owner(at);
        return layer.m_own.contains(layer.local(at), value);
    }
    [[nodiscard]] auto view(handle_type at) const
        requires row_pool
    {
        const auto& layer = owner(at);
        return layer.m_own.view(layer.local(at));
    }
    [[nodiscard]] T get(handle_type at) const
        requires row_pool
    {
        const auto& layer = owner(at);
        return layer.m_own.get(layer.local(at));
    }
    [[nodiscard]] size_type row_size(handle_type at) const
        requires row_pool
    {
        const auto& layer = owner(at);
        return layer.m_own.row_size(layer.local(at));
    }
    void read(handle_type at, std::span<element> out) const
        requires row_pool
    {
        const auto& layer = owner(at);
        layer.m_own.read(layer.local(at), out);
    }

    /// The bytes this layer's own storage takes.
    [[nodiscard]] size_type memory_usage() const { return m_own.memory_usage(); }
    /// Forgets this layer's own elements; the parent's stay, and the indices start again after them.
    void clear()
    {
        m_own.clear();
        m_base = m_parent == nullptr ? 0 : static_cast<std::uint32_t>(m_parent->size());
    }

private:
    /// The layer that holds `at`: this one, or the ancestor whose range it lies in.
    [[nodiscard]] const LayeredPool& owner(handle_type at) const
    {
        // handle_type is R's, so `at.value` is a dependent name: parenthesized, or Clang reads `value <` as urd::value<…>.
        const auto* layer = this;
        while ((at.value) < layer->m_base)
            layer = layer->m_parent;
        assert((at.value) < layer->size() && "urd::repr::LayeredPool: no element at this index");
        return *layer;
    }
    [[nodiscard]] handle_type global(handle_type local_at) const noexcept { return handle_type(m_base + local_at.value); }
    [[nodiscard]] handle_type local(handle_type at) const noexcept { return handle_type(at.value - m_base); }
    /// A row's elements as a vector, from whichever layer holds it.
    [[nodiscard]] std::vector<element> elements_of(handle_type at) const
        requires row_pool
    {
        auto out = std::vector<element>(row_size(at));
        read(at, out);
        return out;
    }

    R m_own;
    const LayeredPool* m_parent = nullptr;
    std::uint32_t m_base = 0;
    std::uint32_t m_layer = 0;
};

}  // namespace urd::repr

namespace urd
{

/// Its own elements as R holds them, plus the parent's in lookups, with indices continuing after the parent's.
template<typename R>
    requires has<container_traits<R>, handles, unique>
struct container_traits<repr::LayeredPool<R>> : with_t<container_traits<R>, layered>
{
};
template<typename R>
struct element_traits<repr::LayeredPool<R>> : element_traits<R>
{
};

/// Its own elements as R holds them, plus the parent's in lookups; in no order, since the parent's elements are
/// not iterated with its own (a dense map's key order holds for each level alone).
template<typename R>
    requires has<container_traits<R>, unique>
struct container_traits<repr::Layered<R>> : with_t<container_traits<R>, layered, any_order>
{
};

}  // namespace urd
