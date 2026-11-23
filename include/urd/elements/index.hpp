#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/requirements.hpp"

#include <boost/mp11.hpp>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace urd
{

/// The number of a Store that hands out indices for views (a pool of records, a positional set): with an index, what
/// an index_view<T, C> is identified by, so that equal views are the same element whichever Store they were made in. Given
/// to the Store's constructor, by whoever builds it: Store(urd::layer { 3 }), or for a layered Store, its place along
/// the chain.
struct layer
{
    std::size_t number;
};

/// What the part of tag T of a key holds, and the value of it that names nothing: by default the number of a T (an
/// index; none its largest value). A library whose store names its records otherwise (a pool's handle of a packed
/// value, whose default names nothing, as a key's does) specializes it for its tag; its key is then that tagged value, and whatever only compares, hashes,
/// stores and resolves keys takes it as it takes an index.
template<typename T>
struct key_value_of
{
    using type = std::uint32_t;
    static constexpr type none = std::numeric_limits<std::uint32_t>::max();
};

template<typename T>
using key_value_t = typename key_value_of<T>::type;

/// Whether the part of tag T is a number (the default): what a dense table keyed by its keys asks.
template<typename T>
concept NumberedKind = std::unsigned_integral<key_value_t<T>>;

namespace detail
{
/// The part of a key that holds the value of T (key_value_of): one flat base per tag (not a std::tuple). A key of one
/// tag reads it as `.value` through its one base; a key of several, where `.value` is ambiguous, through get<T>().
template<typename T>
struct key_part
{
    key_value_t<T> value = key_value_of<T>::none;

    friend constexpr auto operator<=>(const key_part&, const key_part&) = default;
};
}  // namespace detail

/// The key of a record in the context that finds it, one value per tag Ts (key_value_of: a number by default), in order: the first selects a container of
/// the context, the next looks up in it, and so on (a row of a relation: key<Relation, Row>, its relation's number, then
/// its row's there). A key of one tag is the index of a T (urd::key<T>): a positional set or a pool mints one on first
/// insert, so it is dense over the elements; a dense key for tables keyed by Ts. The context resolves a key
/// (context.view(key)), and its views hold it whole (index_view<T, C, key<..., T>>). Trivially copyable, compared and
/// hashed part by part, in order, stored flat by cista part by part; a key of one tag is its one value, as it hashes
/// and orders. The default is none in every part: no key at all, which a dense lookup finds absent. Keys of different
/// tags are different types and convert to none of the others. The key of no part, key<>, is what a record of a key of
/// one part is known by before it is stored (urd::insert_at): nothing.
template<typename... Ts>
struct key : detail::key_part<Ts>...
{
    static_assert(boost::mp11::mp_is_set<boost::mp11::mp_list<Ts...>>::value, "urd::key: each tag once");

    /// The kind it names: its last tag (none, void, for the key of no part).
    using element_type = boost::mp11::mp_eval_if_c<sizeof...(Ts) == 0, void, boost::mp11::mp_back, boost::mp11::mp_list<Ts...>>;
    /// The value of its last part (key_value_of), and the one that names nothing.
    using value_type = key_value_t<element_type>;
    static constexpr value_type none_value = key_value_of<element_type>::none;
    /// Whether it is a key of one tag: an index.
    static constexpr bool single = sizeof...(Ts) == 1;

    constexpr key() = default;

    /// A key of one tag from its number.
    constexpr explicit key(value_type at) noexcept
        requires single
        : detail::key_part<Ts> { at }...
    {
    }

    /// A key of several tags from its parts, in order.
    /// (Taken as their parts' bases: a key<Ts> of one tag is one.)
    constexpr explicit key(const detail::key_part<Ts>&... parts) noexcept
        requires(sizeof...(Ts) > 1)
        : detail::key_part<Ts> { parts }...
    {
    }

    /// The part of tag T, as a key of T.
    template<typename T>
    [[nodiscard]] constexpr key<T> get() const noexcept
    { return key<T>(static_cast<const detail::key_part<T>&>(*this).value); }

    /// Whether it names nothing: every part none.
    [[nodiscard]] constexpr bool is_none() const noexcept { return ((static_cast<const detail::key_part<Ts>&>(*this).value == key_value_of<Ts>::none) && ...); }

    friend constexpr auto operator<=>(const key&, const key&) = default;

    static constexpr auto identifying_members() noexcept { return Members<&detail::key_part<Ts>::value...> {}; }

    /// For records stored flat: cista serializes a key part by part.
    static constexpr auto cista_members() noexcept { return Members<&detail::key_part<Ts>::value...> {}; }
};

/// The index of a T in the Store that numbers Ts: the key of one tag of a numbered kind (its value a dense number), the
/// special form of a key. A key whose value is a store's handle (key_value_of) is no index: it is written key<T>.
template<NumberedKind T>
using index = key<T>;

/// The key a record of T is stored under: its index, unless a library says otherwise by specializing key_of<T> (a
/// record numbered within another, as a row within its relation: key<F, Row<F>>).
template<typename T>
struct key_of
{
    using type = key<T>;
};

template<typename T>
using key_t = typename key_of<T>::type;

/// The part of tag T of a key, as a key of T: `urd::get<Relation>(row_key)`.
template<typename T, typename... Ts>
    requires(std::same_as<T, Ts> || ...)
[[nodiscard]] constexpr key<T> get(const key<Ts...>& at) noexcept
{ return at.template get<T>(); }

/// An index of a numbered kind is a dense key; none is no index, so a lookup of it finds nothing.
template<NumberedKind T>
struct index_of<index<T>>
{
    [[nodiscard]] static constexpr bool is_index(index<T> at) noexcept { return !at.is_none(); }

    [[nodiscard]] static constexpr std::size_t to_index(index<T> at) noexcept
    {
        assert(is_index(at) && "urd::index_of: none is no index");
        return at.value;
    }

    [[nodiscard]] static constexpr index<T> from_index(std::size_t at) noexcept { return index<T>(static_cast<std::uint32_t>(at)); }
};

/// Whether K is a key<...> (of one tag, an index, or of several).
template<typename K>
inline constexpr bool is_key_type = false;
template<typename... Ts>
inline constexpr bool is_key_type<key<Ts...>> = true;

/// A key: what names a record in its context (an index, or a key of several parts).
template<typename K>
concept Key = is_key_type<K>;

/// A change to a row of a pool: the value at `position` becomes `value`. A pool of sequence rows takes a sorted
/// span of them, with unique positions, to insert a row that differs from another in few positions.
template<typename T>
struct Write
{
    std::size_t position = 0;
    T value {};
};

/// The Store or layer that resolves a T's indices by default: `template<> struct urd::context_of<Atom> { using
/// type = Repository; };` once per record type, so that index_view<Atom> needs no second argument.
template<typename T>
struct context_of;

/// The stored form of a T (urd/elements/data.hpp).
template<typename T>
struct data;

/// Whether a record of T holds its key, as an interned record does: its key() (key_t<T>: its index, the local part its
/// store writes, after the parts that select that store), the one way urd reads it.
template<typename T>
concept SelfIndexed = requires(const data<T>& record) {
    { record.key() } -> std::same_as<key_t<T>>;
};

/// A T read through a context C, which it borrows (urd/elements/view.hpp): index_view holds an index K (key<T>, or a
/// handle of another shape the context resolves) and reads it through the context; borrowed_view, owning_view and
/// shared_view hold a record R (data<T>, or builder<T, C>) outside the context: a pointer to it, it by value, a shared
/// pointer to it.
template<typename T, typename C = typename context_of<T>::type, typename K = key<T>>
class index_view;
template<typename T, typename C = typename context_of<T>::type, typename R = data<T>>
class borrowed_view;
template<typename T, typename C = typename context_of<T>::type, typename R = data<T>>
class owning_view;
template<typename T, typename C = typename context_of<T>::type, typename R = data<T>>
class shared_view;

/// Whether a handle names nothing: the one way generic code asks a handle (an index, a tree pool's handle, ...), found by
/// argument-dependent lookup, so that a handle that is a value (data<T>, no member functions) answers too.
template<typename... Ts>
[[nodiscard]] constexpr bool is_none(const key<Ts...>& at) noexcept
{ return at.is_none(); }

}  // namespace urd
