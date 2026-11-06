/*
    Adapted from nanobind/stl (vector.h, string.h, variant.h, optional.h, pair.h).

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

// The flat forms of records (urd/elements/flat.hpp: cista's offset containers) convert to and from Python like their
// std counterparts.

#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>
#include <cstddef>
#include <nanobind/nanobind.h>
#include <nanobind/stl/detail/nb_list.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/variant.h>
#include <string_view>
#include <type_traits>
#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<typename Type, template<typename> typename Ptr, bool IndexPointers, typename TemplateSizeType, class Allocator>
struct type_caster<::cista::basic_vector<Type, Ptr, IndexPointers, TemplateSizeType, Allocator>> :
    list_caster<::cista::basic_vector<Type, Ptr, IndexPointers, TemplateSizeType, Allocator>, Type>
{
};

// Taken From nanobind/stl/string.h
template<typename Ptr>
struct type_caster<cista::basic_string<Ptr>>
{
private:
    using String = cista::basic_string<Ptr>;
    using CharT = typename String::CharT;

public:
    // Minimal: only support cista strings over char
    static_assert(std::is_same_v<std::remove_cv_t<CharT>, char>, "nanobind caster: only CharT=char is supported");

    NB_TYPE_CASTER(String, const_name("str"))

    bool from_python(handle src, uint32_t, cleanup_list*) noexcept
    {
        Py_ssize_t size = 0;
        const char* str = PyUnicode_AsUTF8AndSize(src.ptr(), &size);
        if (!str)
        {
            PyErr_Clear();
            return false;
        }
        value = String(std::string_view(str, static_cast<size_t>(size)));  // owning copy
        return true;
    }

    static handle from_cpp(const String& s, rv_policy, cleanup_list*) noexcept
    { return PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())); }
};

// Adapted from nanobind/stl/variant.h
template<typename... Ts>
struct type_caster<::cista::offset::variant<Ts...>>
{
    using Variant = ::cista::offset::variant<Ts...>;

    // Cista variants are default-constructible even if their alternatives are not.
    NB_TYPE_CASTER(Variant, union_name(make_caster<Ts>::Name...))

    template<typename T>
    bool try_variant(const handle& src, uint32_t flags, cleanup_list* cleanup)
    {
        using CasterT = make_caster<T>;

        CasterT caster;

        if (!caster.from_python(src, flags_for_local_caster<T>(flags), cleanup) || !caster.template can_cast<T>())
            return false;

        value = caster.operator cast_t<T>();

        return true;
    }

    bool from_python(handle src, uint32_t flags, cleanup_list* cleanup) noexcept
    {
        if (flags & static_cast<uint32_t>(cast_flags::convert))
        {
            if ((try_variant<Ts>(src, flags & ~static_cast<uint32_t>(cast_flags::convert), cleanup) || ...))
            {
                return true;
            }
        }
        return (try_variant<Ts>(src, flags, cleanup) || ...);
    }

    template<typename T>
    static handle from_cpp(T&& value, rv_policy policy, cleanup_list* cleanup) noexcept
    {
        if (!value.valid())
            return none().release();

        // The static apply has no empty case, which valid() above has handled.
        const auto index = value.index();
        return std::remove_cvref_t<T>::apply([&]<typename V>(V&& v) { return make_caster<V>::from_cpp(std::forward<V>(v), policy, cleanup); },
                                             index,
                                             std::forward<T>(value));
    }
};

// Taken from nanobind/stl/optional.h
template<typename T>
struct type_caster<::cista::optional<T>> : optional_caster<::cista::optional<T>>
{
};

// Reuse Nanobind's pair conversions, constructing a Cista pair on input.
template<typename T1, typename T2>
struct type_caster<::cista::pair<T1, T2>> : type_caster<std::pair<T1, T2>>
{
    using Value = ::cista::pair<T1, T2>;

    template<typename T>
    using Cast = Value;

    explicit operator Value() { return Value { this->caster1.operator cast_t<T1>(), this->caster2.operator cast_t<T2>() }; }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
