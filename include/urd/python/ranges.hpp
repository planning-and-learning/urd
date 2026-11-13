/*
    Adapted from nanobind/stl (detail/nb_list.h).

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

// Views (spans, ranges read through a context: urd::resolve) convert to Python lists.

#include <concepts>
#include <nanobind/nanobind.h>
#include <nanobind/stl/detail/nb_list.h>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// The elements of `range` (a view the caster copied: a filter is iterable only when it is not const) as a list.
template<typename Caster, typename R>
handle sequence_view_from_cpp(R&& range, rv_policy policy, cleanup_list* cleanup)
{
    object result = steal(PyList_New(0));
    if (!result.is_valid())
        return {};
    for (auto&& value : std::forward<R>(range))
    {
        const object item = steal(Caster::from_cpp(std::forward<decltype(value)>(value), policy, cleanup));
        if (!item.is_valid() || PyList_Append(result.ptr(), item.ptr()) != 0)
            return {};
    }
    return result.release();
}

// A view (a span, a range read through a context: transformed, filtered or borrowed, never owning) as a list of its
// elements. Adapted from nanobind/stl/detail/nb_list.h.
template<typename R>
    requires std::ranges::view<R> && std::ranges::input_range<R>
struct type_caster<R, int>
{
private:
    using Entry = std::remove_cvref_t<std::ranges::range_reference_t<R>>;
    using Caster = make_caster<Entry>;

public:
    NB_TYPE_CASTER(R, io_name("collections.abc.Sequence", "list") + const_name("[") + Caster::Name + const_name("]"))

    // Views are read-only: there is no storage to build one in from Python.
    bool from_python(handle, uint32_t, cleanup_list*) noexcept { return false; }

    template<typename T>
    static handle from_cpp(T&& src, rv_policy policy, cleanup_list* cleanup)
    { return sequence_view_from_cpp<Caster>(R(std::forward<T>(src)), policy, cleanup); }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
