#pragma once

// A pool over a parent pool: its own rows, numbered after the parent's.

#include "urd/elements/index.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/pools/pools_fwd.hpp"
#include "urd/repr/records/records_fwd.hpp"
#include "urd/repr/slots/slots_fwd.hpp"
#include "urd/repr/wrappers/layered_fwd.hpp"
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
    }
    [[nodiscard]] const LayeredPool* parent() const noexcept { return m_parent; }
    /// Where this layer's own indices start: the parent's size at the branch.
    [[nodiscard]] size_type base() const noexcept { return m_base; }
    /// Whether the parent grew after the branch, which it must not.
    [[nodiscard]] bool parent_grew() const noexcept { return m_parent != nullptr && m_parent->size() > m_base; }

    [[nodiscard]] size_type size() const noexcept { return m_base + m_own.size(); }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // Records. A record is hashed once (urd::prehash) and found by that hash in every layer of the chain.

    std::pair<handle_type, bool> insert(const T& record)
        requires(!row_pool)
    { return insert(prehash(record)); }
    std::pair<handle_type, bool> insert(const prehashed<T>& key)
        requires(!row_pool)
    { return insert(key, [&key](handle_type) -> const T& { return key.key; }); }
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const T& key, Factory make)
        requires(!row_pool)
    { return insert(prehash(key), std::move(make)); }
    template<std::invocable<handle_type> Factory>
    std::pair<handle_type, bool> insert(const prehashed<T>& key, Factory make)
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
    { return find(prehash(record)); }
    [[nodiscard]] handle_type find(const prehashed<T>& key) const
        requires(!row_pool)
    { return find_in_chain(key); }
    [[nodiscard]] bool contains(const T& record) const
        requires(!row_pool)
    { return !find(record).is_none(); }
    [[nodiscard]] bool contains(const prehashed<T>& key) const
        requires(!row_pool)
    { return !find(key).is_none(); }
    [[nodiscard]] const T& operator[](handle_type at) const
        requires(!row_pool)
    {
        const auto& layer = owner(at);
        return layer.m_own[layer.local(at)];
    }

    // Rows.

    /// The handle, and whether the row was stored now: in this layer, as no layer of the chain held it.
    template<std::ranges::input_range Elements>
    std::pair<handle_type, bool> insert(const Elements& elements)
        requires row_pool
    { return insert_row(elements); }
    /// By a row with its hash (urd::prehashed: the hash urd::hash_range gives its elements), where the parent pool
    /// takes one: hashed once for the whole chain, where a row given as it is is hashed again in every layer.
    template<typename Elements>
        requires row_pool && requires(R& own, const prehashed<Elements>& key) { own.insert(key); }
    std::pair<handle_type, bool> insert(const prehashed<Elements>& key)
    { return insert_row(key); }
    /// The row `base` with `writes` applied, stored here where new.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const write_type> writes)
        requires row_pool && requires(R& own, handle_type at, std::span<const write_type> some) { own.insert(at, some); }
    {
        auto row = elements_of(base);
        for (const auto& write : writes)
            row.at(write.position) = write.value;
        return insert(row);
    }
    /// The row `base` with `added` put in and `removed` taken out, stored here where new.
    std::pair<handle_type, bool> insert(handle_type base, std::span<const element> added, std::span<const element> removed)
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
    { return find_in_chain(elements); }
    template<typename Elements>
        requires row_pool && requires(const R& own, const prehashed<Elements>& key) { own.find(key); }
    [[nodiscard]] handle_type find(const prehashed<Elements>& key) const
    { return find_in_chain(key); }
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

    /// Forgets this layer's own elements; the parent's stay, and the indices start again after them.
    void clear()
    {
        m_own.clear();
        m_base = m_parent == nullptr ? 0 : static_cast<std::uint32_t>(m_parent->size());
    }

private:
    /// The handle of `key` (a record or row, or it prehashed) in the nearest layer of the chain that holds it, or none.
    template<typename K>
    [[nodiscard]] handle_type find_in_chain(const K& key) const
    {
        for (const auto* layer = this; layer != nullptr; layer = layer->m_parent)
            if (const auto at = layer->m_own.find(key); !at.is_none())
                return layer->global(at);
        return handle_type();
    }
    /// The row `key` (its elements, or them prehashed) found in the chain, else stored here.
    template<typename K>
    std::pair<handle_type, bool> insert_row(const K& key)
    {
        if (const auto at = find_in_chain(key); !at.is_none())
            return { at, false };
        assert(!parent_grew() && "urd::repr::LayeredPool: the parent grew after the branch");
        const auto [own, stored] = m_own.insert(key);
        return { global(own), stored };
    }
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

    friend struct urd::memory_of<LayeredPool>;

    R m_own;
    const LayeredPool* m_parent = nullptr;
    std::uint32_t m_base = 0;
};

}  // namespace urd::repr

namespace urd
{

/// Its own layer's: the parent's is the parent's.
template<typename R>
struct memory_of<repr::LayeredPool<R>>
{
    static std::size_t bytes(const repr::LayeredPool<R>& held) { return memory_usage(held.m_own); }
};

template<>
struct include<"urd/repr/wrappers/layered_pool.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

/// Over its part's container for the request.
template<typename Part, typename S>
struct representation_of<layered_decl<Part, true>, S>
{
    using type = repr::LayeredPool<typename representation_of<Part, S>::type>;
};

}  // namespace urd::detail
