#pragma once

#include "urd/index.hpp"
#include "urd/store/types.hpp"
#include "urd/traits.hpp"

#include <concepts>
#include <cstddef>
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

    /// The record's index, and whether it was new: an equal record already held gives its index.
    std::pair<index_type, bool> insert(const T& record) { return representation().insert(record); }
    /// The index `key` gets, and whether it was new: where it is, the record stored is what `make(index)` gives,
    /// built with the index reserved for it (a record that holds its own index). It must equal `key`, and `make`
    /// must not touch this Store.
    template<std::invocable<index_type> Factory>
        requires std::convertible_to<std::invoke_result_t<Factory, index_type>, T>
    std::pair<index_type, bool> insert(const T& key, Factory make)
    {
        return representation().insert(key, make);
    }
    /// The index of an equal record, or none.
    [[nodiscard]] index_type find(const T& record) const { return constant().find(record); }
    [[nodiscard]] bool contains(const T& record) const { return constant().contains(record); }
    [[nodiscard]] bool contains(index_type at) const { return !at.is_none() && at.value < constant().size(); }
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
        return urd::view<T, Derived>(at, static_cast<const Derived&>(*this));
    }
    [[nodiscard]] std::size_t memory_usage() const { return constant().memory_usage(); }
    void clear()
        requires has<S, shrinkable>
    {
        representation().clear();
    }

private:
    [[nodiscard]] const R& constant() const { return store_access::representation(static_cast<const Derived&>(*this)); }
    R& representation() { return store_access::representation(static_cast<Derived&>(*this)); }
};

}  // namespace urd::detail::facet
