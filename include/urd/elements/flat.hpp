#pragma once

// Records stored flat: the cista containers a record is written with, under urd's names, their hash and equality,
// and how a view resolves them. Include it where flat records are defined; the pools that store them are in
// urd/repr/records/records.hpp and urd/repr/records/arena.hpp, included where such a Store is held.

#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/view.hpp"
#include "urd/memory.hpp"

#include <boost/mp11/algorithm.hpp>
#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace urd
{

/// The containers of a record stored flat: cista's, which serialize as a whole.
template<typename T>
using flat_vector = ::cista::offset::vector<T>;
using flat_string = ::cista::offset::string;
template<typename... Ts>
using flat_variant = ::cista::offset::variant<Ts...>;
template<typename T>
using flat_optional = ::cista::optional<T>;
template<typename T1, typename T2>
using flat_pair = ::cista::pair<T1, T2>;

namespace detail
{
template<typename T>
inline constexpr bool flat_vector_type = false;
template<typename T>
inline constexpr bool flat_vector_type<flat_vector<T>> = true;
template<typename T>
inline constexpr bool flat_variant_type = false;
template<typename... Ts>
inline constexpr bool flat_variant_type<flat_variant<Ts...>> = true;
template<typename T>
inline constexpr bool flat_optional_type = false;
template<typename T>
inline constexpr bool flat_optional_type<flat_optional<T>> = true;
template<typename T>
inline constexpr bool flat_pair_type = false;
template<typename T1, typename T2>
inline constexpr bool flat_pair_type<flat_pair<T1, T2>> = true;
}  // namespace detail

/// Whether T is one of the flat containers above.
template<typename T>
concept FlatVector = detail::flat_vector_type<T>;
template<typename T>
concept FlatVariant = detail::flat_variant_type<T>;
template<typename T>
concept FlatOptional = detail::flat_optional_type<T>;
template<typename T>
concept FlatPair = detail::flat_pair_type<T>;
/// The flat_variant of the indices of a type list's tags.
template<typename L>
using index_variant = boost::mp11::mp_apply<flat_variant, boost::mp11::mp_transform<index, L>>;

/// Text hashes as its bytes (Hash<std::string_view>), so equal text hashes alike in every representation; it compares
/// by cista's ==.
template<>
struct Hash<flat_string>
{
    hash_t operator()(const flat_string& text) const noexcept { return Hash<std::string_view> {}(text.view()); }
};

/// Text owns its characters where they do not fit inside it (cista allocates exactly its length).
template<>
struct memory_of<flat_string>
{
    static std::size_t bytes(const flat_string& text) { return text.is_short() ? 0 : text.size(); }
};

/// A flat vector owns the elements it allocated (none where it reads a buffer it was loaded from), and what they own.
template<typename T>
struct memory_of<flat_vector<T>>
{
    static std::size_t bytes(const flat_vector<T>& held)
    { return (held.self_allocated_ ? held.allocated_size_ * sizeof(T) : std::size_t { 0 }) + detail::held_bytes(held); }
};

/// A record that names its members (as flat storage needs) holds what they hold.
template<Serializable T>
struct memory_of<T>
{
    static std::size_t bytes(const T& record)
    {
        return []<auto... Ms>(Members<Ms...> /*members*/, const T& of) { return (std::size_t { 0 } + ... + memory_usage(of.*Ms)); }(T::cista_members(), record);
    }
};

/// A flat vector is a value range: hashed, compared and ordered as its elements.
template<typename T, template<typename> typename Ptr, bool IndexPointers, typename SizeType, class Allocator>
inline constexpr bool value_range<::cista::basic_vector<T, Ptr, IndexPointers, SizeType, Allocator>> = true;

/// A readout written as a member of type M: M's own rule, the inverse of how M is read (resolution<M>::stored). A view
/// of an element gives its index, a range of readouts M's list of them, an optional or a variant of readouts M's
/// optional or variant (each alternative to the one of M's that takes it), a pair its members; a plain value only an M
/// itself, without conversion. A readout M does not take is a compile error naming M.
template<typename M, typename R>
    requires stores<M, R>
[[nodiscard]] M as_member(const R& readout)
{ return resolution<M>::stored(readout); }

/// Writes a readout into a member, as the member's type stores it (urd::as_member).
template<typename M, typename R>
    requires stores<M, R>
void store(M& member, const R& readout)
{ member = resolution<M>::stored(readout); }

/// Appends a readout to a list member, as its element type stores it (urd::as_member).
template<typename List, typename R>
    requires stores<std::ranges::range_value_t<List>, R>
void append(List& list, const R& readout)
{ list.push_back(resolution<std::ranges::range_value_t<List>>::stored(readout)); }

/// The record data<T> from one readout per field, in field order (fields()), each written as its field's type
/// (urd::store); its index, if it has one, is none.
template<typename T, typename... Readouts>
[[nodiscard]] data<T> make(const Readouts&... readouts)
{
    auto result = data<T>();
    [&]<typename... Fs>(Fields<Fs...> /*fields*/)
    {
        static_assert(sizeof...(Fs) == sizeof...(Readouts), "urd::make: one readout per field of the record, in field order");
        (urd::store(result.*Fs::member, readouts), ...);
    }(data<T>::fields());
    return result;
}

}  // namespace urd
