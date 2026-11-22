#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/less.hpp"

#include <compare>

namespace urd
{

/// The stored form of a T: the record a Store holds for an index<T> and a view's data() reads. Specialize it per type;
/// it must be Serializable (cista_members() names its members as urd::Members). urd::index_view<T, C> reads it through
/// its context; urd::borrowed_view<T, C> points at one held outside a context, urd::owning_view<T, C> holds one,
/// urd::shared_view<T, C> shares one (urd/elements/view.hpp).
template<typename T>
struct data;

/// Whether data<T> is a record urd reads: Serializable.
template<typename T>
concept stored_record = Serializable<data<T>>;

/// The record a store holds for T: data<T> where T has a stored record (stored_record), else a plain T (a Store of plain
/// values, read whole).
template<typename T>
struct record_form
{
    using type = T;
};

template<typename T>
    requires stored_record<T>
struct record_form<T>
{
    using type = data<T>;
};

/// A record of T where a store holds it, handed out by value: what a lookup returns by default (stored_ref<T>).
/// Trivially copyable; the record must outlive it.
template<typename T>
class record_ref
{
    const typename record_form<T>::type* m_record;

public:
    explicit record_ref(const typename record_form<T>::type& record) noexcept : m_record(&record) {}

    [[nodiscard]] const typename record_form<T>::type& operator*() const noexcept { return *m_record; }

    [[nodiscard]] const typename record_form<T>::type* operator->() const noexcept { return m_record; }
};

/// What a lookup of a T hands out, by value (urd::record_at, a context's operator[] and at()): record_ref<T> unless T
/// names another, by specializing stored_ref_of<T> (a row packed in its slot: a reader over its bits). The one
/// customization point of how a record is handed out.
template<typename T>
struct stored_ref_of
{
    using type = record_ref<T>;
};

template<typename T>
using stored_ref = typename stored_ref_of<T>::type;

/// A field of a record, read through the stored_ref its store hands out (R): `(*ref).*Member`, in place, for a record
/// held whole (record_ref); a stored form of its own (a row packed in its slot) says how it reads each field of data<T>
/// by specializing stored_field.
template<auto Member, typename R>
struct stored_field
{
    [[nodiscard]] static const member_type_t<Member>& read(const R& ref) { return (*ref).*Member; }
};

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
