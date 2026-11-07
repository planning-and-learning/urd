#pragma once

// A view (urd::index_view, borrowed_view, owning_view, shared_view) handed to Python keeps its context alive: the
// context's own Python object where it has one (nb::find), else the self of the call that returned the view (nanobind
// hands it to every caster through the cleanup list; nb::cast(value, policy, parent) names it), whose context is a part
// of it, or the view it was read from. Every view goes through this caster, so it holds for views inside a returned
// list, tuple or dict, yielded by an iterator or passed to a callback too. Views reach Python by value, where
// nanobind's reference_internal does not apply (it becomes move).

#include "urd/elements/view.hpp"

#include <nanobind/nanobind.h>
#include <type_traits>
#include <utility>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template<typename V>
struct urd_view_caster : type_caster_base<V>
{
    template<typename U>
    static handle from_cpp(U&& value, rv_policy policy, cleanup_list* cleanup) noexcept
    {
        // nanobind passes a view by value, by reference, or by pointer (a reference return).
        const V* view = nullptr;
        if constexpr (std::is_pointer_v<std::remove_cvref_t<U>>)
            view = value;
        else
            view = &value;
        object context;
        if (view)
        {
            context = find(view->context());
            if (!context.is_valid())
            {
                PyErr_Clear();  // the context is not a Python object of its own
                if (cleanup && cleanup->self())
                    context = borrow(cleanup->self());
            }
        }
        handle result = type_caster_base<V>::from_cpp(std::forward<U>(value), policy, cleanup);
        if (result.is_valid() && context.is_valid())
            NB_CALL(keep_alive_py)(NB_CTX_C(cleanup), result.ptr(), context.ptr());
        return result;
    }
};

template<typename T, typename C, typename H>
struct type_caster<urd::index_view<T, C, H>> : urd_view_caster<urd::index_view<T, C, H>>
{
};
template<typename T, typename C, typename R>
struct type_caster<urd::borrowed_view<T, C, R>> : urd_view_caster<urd::borrowed_view<T, C, R>>
{
};
template<typename T, typename C, typename R>
struct type_caster<urd::owning_view<T, C, R>> : urd_view_caster<urd::owning_view<T, C, R>>
{
};
template<typename T, typename C, typename R>
struct type_caster<urd::shared_view<T, C, R>> : urd_view_caster<urd::shared_view<T, C, R>>
{
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
