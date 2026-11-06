#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/less.hpp"

#include <compare>

namespace urd
{

/// The stored form of a T: the record a Store holds for an index<T> and a view's data() reads. Specialize it per type;
/// it must be Serializable (cista_members() names its members as urd::Members). urd::view<T, C> reads it through its
/// context; urd::borrowed_view<T, C> borrows one held outside a context, urd::owned_view<T, C> owns one computed on the
/// fly (urd/elements/view.hpp).
template<typename T>
struct data;

/// Whether data<T> is a record urd reads: Serializable.
template<typename T>
concept stored_record = Serializable<data<T>>;

/// A record with identifying_members() compares by them.
template<typename T>
    requires Identifiable<data<T>>
constexpr bool operator==(const data<T>& lhs, const data<T>& rhs)
{ return EqualTo<data<T>> {}(lhs, rhs); }

template<typename T>
    requires Identifiable<data<T>> && std::invocable<ThreeWayCompare<data<T>>, const data<T>&, const data<T>&>
constexpr std::strong_ordering operator<=>(const data<T>& lhs, const data<T>& rhs)
{ return ThreeWayCompare<data<T>> {}(lhs, rhs); }

}  // namespace urd
