#pragma once

#include "urd/elements/packed_row.hpp"
#include "urd/python/ranges.hpp"

#include <cstdint>
#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

/// A packed row (a stored row's values, read through its pool's widths) as a list of its canonical values. Read-only.
template<bool ThreadSafe>
struct type_caster<::urd::repr::SchemaRow<ThreadSafe>>
{
private:
    using Row = ::urd::repr::SchemaRow<ThreadSafe>;
    using Caster = make_caster<typename Row::value_type>;

public:
    NB_TYPE_CASTER(Row, io_name("collections.abc.Sequence", "list") + const_name("[") + Caster::Name + const_name("]"))

    bool from_python(handle /*src*/, uint32_t /*flags*/, cleanup_list* /*cleanup*/) noexcept { return false; }

    static handle from_cpp(const Row& src, rv_policy policy, cleanup_list* cleanup) { return sequence_view_from_cpp<Caster>(src, policy, cleanup); }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
