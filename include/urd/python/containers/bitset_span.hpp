#pragma once

// A bitset_span (urd/repr/containers/bitset_span.hpp) converts to Python as a list of its bits, copied: a span borrows
// the blocks of its owner, which Python does not keep, so a copy is what outlives the call. len() is its size, [i] its
// test(i), sum() its count(), any() its any(). Only to Python: a list builds no blocks a span could borrow.

#include "urd/repr/containers/bitset_span.hpp"

#include <cstddef>
#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<typename Block>
struct type_caster<urd::repr::BitsetSpan<Block>>
{
    static constexpr auto Name = const_name("list[bool]");
    template<typename T>
    using Cast = urd::repr::BitsetSpan<Block>;

    /// Through the C API, as nanobind's own casters: an empty handle (Python's error set) where the list cannot be made.
    static handle from_cpp(const urd::repr::BitsetSpan<Block>& bits, rv_policy /*policy*/, cleanup_list* /*cleanup*/) noexcept
    {
        PyObject* list = PyList_New(static_cast<Py_ssize_t>(bits.size()));
        if (list == nullptr)
            return {};
        for (std::size_t position = 0; position < bits.size(); ++position)
        {
            PyObject* bit = bits.test(position) ? Py_True : Py_False;
            Py_INCREF(bit);
            // PyList_SetItem (not the macro, which the limited API lacks) takes the reference, also on failure.
            if (PyList_SetItem(list, static_cast<Py_ssize_t>(position), bit) != 0)
            {
                Py_DECREF(list);
                return {};
            }
        }
        return list;
    }
};
NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
