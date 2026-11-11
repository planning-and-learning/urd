/*
    Adapted from nanobind/stl (detail/nb_list.h).

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

// Ranges read through a context (urd::resolve) convert to Python lists.

#include <concepts>
#include <nanobind/nanobind.h>
#include <nanobind/stl/detail/nb_list.h>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<typename Caster, typename T>
handle sequence_view_from_cpp(T&& src, rv_policy policy, cleanup_list* cleanup)
{
    object ret = steal(PyList_New(static_cast<Py_ssize_t>(src.size())));

    if (ret.is_valid())
    {
        Py_ssize_t index = 0;

        for (auto&& value : std::forward<T>(src))
        {
            handle h = Caster::from_cpp(forward_like_<T>(value), policy, cleanup);

            if (!h.is_valid())
            {
                ret.reset();
                break;
            }

            NB_LIST_SET_ITEM(ret.ptr(), index++, h.ptr());
        }
    }

    return ret.release();
}

// A range read through a context (urd::resolve: transformed or borrowed, never owning) as a list of its elements.
// Adapted from nanobind/stl/detail/nb_list.h.
template<typename T>
inline constexpr bool is_std_span_v = false;
template<typename T, std::size_t Extent>
inline constexpr bool is_std_span_v<std::span<T, Extent>> = true;

// Spans are bound as classes where they are exposed (nb::class_<std::span<...>>), so they are left to those bindings.
template<typename R>
    requires std::ranges::view<R> && std::ranges::sized_range<const R> && (!is_std_span_v<R>)
struct type_caster<R, int>
{
private:
    using Entry = std::remove_cvref_t<std::ranges::range_reference_t<const R>>;
    using Caster = make_caster<Entry>;

public:
    NB_TYPE_CASTER(R, io_name("collections.abc.Sequence", "list") + const_name("[") + Caster::Name + const_name("]"))

    // Views are read-only: there is no storage to build one in from Python.
    bool from_python(handle, uint32_t, cleanup_list*) noexcept { return false; }

    template<typename T>
    static handle from_cpp(T&& src, rv_policy policy, cleanup_list* cleanup)
    { return sequence_view_from_cpp<Caster>(std::forward<T>(src), policy, cleanup); }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
