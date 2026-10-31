#pragma once

#include "urd/index.hpp"
#include "urd/store/denials.hpp"
#include "urd/store/types.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace urd::detail::facet
{

/// A pool of records stored flat (handles, unique, flat): each distinct record once, as one block of bytes, under
/// the index insert gave it; read back by index, looked up by record, and counted.
template<typename Derived, typename R, typename S>
class records
{
    using T = typename S::element_type;

public:
    using record_type = T;
    using index_type = urd::index<T>;
    using handle_type = index_type;

    /// The records in index order, 0 to size() - 1, each read by its index.
    class const_iterator
    {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        const_iterator() = default;
        const_iterator(const R* records, std::uint32_t at) : m_records(records), m_at(at) {}

        [[nodiscard]] const T& operator*() const { return (*m_records)[index_type(m_at)]; }
        const_iterator& operator++()
        {
            ++m_at;
            return *this;
        }
        const_iterator operator++(int)
        {
            auto before = *this;
            ++m_at;
            return before;
        }
        friend bool operator==(const const_iterator&, const const_iterator&) = default;

    private:
        const R* m_records = nullptr;
        std::uint32_t m_at = 0;
    };
    /// O(n): the records lie in one store, each at its index. Not where layered: there a record is reached through
    /// the layer that owns it.
    [[nodiscard]] const_iterator begin() const
        requires(!has<S, layered>)
    { return { &constant(), 0 }; }
    [[nodiscard]] const_iterator end() const
        requires(!has<S, layered>)
    { return { &constant(), static_cast<std::uint32_t>(constant().size()) }; }
    template<typename... Args>
        requires has<S, layered> && denied<"begin/end: a layered pool reaches each record through the layer that owns it; walk the indices">
    void begin(Args&&...) const;
    template<typename... Args>
        requires has<S, layered> && denied<"begin/end: a layered pool reaches each record through the layer that owns it; walk the indices">
    void end(Args&&...) const;

    /// The record's index, and whether it was new: an equal record already held gives its index.
    std::pair<index_type, bool> insert(const T& record) { return representation().insert(record); }
    /// The index `key` gets, and whether it was new: where it is, the record stored is what `make(index)` gives,
    /// built with the index reserved for it (a record that holds its own index). It must equal `key`, and `make`
    /// must not touch this Store.
    template<std::invocable<index_type> Factory>
        requires std::convertible_to<std::invoke_result_t<Factory, index_type>, T>
    std::pair<index_type, bool> insert(const T& key, Factory make)
    { return representation().insert(key, make); }
    /// The index of an equal record, or none.
    [[nodiscard]] index_type find(const T& record) const { return constant().find(record); }
    [[nodiscard]] bool contains(const T& record) const { return constant().contains(record); }
    [[nodiscard]] std::size_t count(const T& record) const { return contains(record) ? 1 : 0; }
    [[nodiscard]] bool contains(index_type at) const { return !at.is_none() && (at.value) < constant().size(); }
    [[nodiscard]] const T& operator[](index_type at) const { return constant()[at]; }
    [[nodiscard]] const T& at(index_type at) const
    {
        if (!contains(at))
            throw std::out_of_range("urd::Store::at: no record at this index");
        return (*this)[at];
    }
    /// A view of the record at `at`, through this Store (urd/view.hpp must be included where views are made).
    [[nodiscard]] auto view(index_type at) const
    {
        static_assert(requires { sizeof(urd::view<T, Derived>); }, "urd::Store::view: include <urd/view.hpp>");
        if constexpr (has<S, layered>)
            return urd::view<T, Derived>(at, static_cast<const Derived&>(*this).owner(at));
        else
            return urd::view<T, Derived>(at, static_cast<const Derived&>(*this));
    }
    [[nodiscard]] std::size_t memory_usage() const { return constant().memory_usage(); }
    void clear()
        requires has<S, shrinkable>
    { representation().clear(); }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet

namespace urd::detail
{

/// When a Store has this facet: from its spec S.
template<>
struct facet_condition<facet_id::records>
{
    static constexpr bool reads_container = false;
    template<typename R, typename S>
    static constexpr bool on()
    { return S::is_pool && has<S, unique> && has<S, flat>; }
};

}  // namespace urd::detail
