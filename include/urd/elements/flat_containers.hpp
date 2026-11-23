#ifndef URD_ELEMENTS_FLAT_CONTAINERS_HPP_
#define URD_ELEMENTS_FLAT_CONTAINERS_HPP_

// The cista containers a record is written with, under urd's names, and the concepts naming them; their hash,
// equality and how a view resolves them are in urd/elements/flat.hpp.

#include "urd/elements/index.hpp"

#include <boost/mp11/algorithm.hpp>
#include <cista/containers/optional.h>
#include <cista/containers/pair.h>
#include <cista/containers/string.h>
#include <cista/containers/variant.h>
#include <cista/containers/vector.h>

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

}  // namespace urd

#endif
