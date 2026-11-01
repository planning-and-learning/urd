#pragma once

// Containers for keys that are indices 0..n-1 (dense keys), declared with what the registry reads of them: a bitset,
// a sparse set, a slot-per-index map. Defined in urd/repr/values/dense_set.hpp, sparse_set.hpp and index_map.hpp. The registry
// names them for any element type, so they are unconstrained templates; their container_traits declare properties
// only for indexable keys.

#include "urd/costs.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/registry/derived.hpp"
#include "urd/traits.hpp"
#include "urd/type_traits_of.hpp"

#include <type_traits>

namespace urd::repr
{

/// A set of indices as a bitset: a bit per key below the largest, iterated in key order; with TrackWords, clear
/// zeroes only the words that are not zero.
template<typename K, bool TrackWords = false>
class DenseSet;

/// A set of indices as a sparse set (Briggs and Torczon): the members in one array, their places by index; with
/// KeepOrder, the members stay in insertion order.
template<typename K, bool KeepOrder = false>
class SparseSet;

}  // namespace urd::repr

namespace urd
{

namespace detail
{

/// dense set (a bitset over indices)
struct dense_set_decl
{
    static constexpr std::string_view id = "dense_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/dense_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, keys::dense, representation::canonical, memory::retains, clearing::none });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_keys;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 0.016;
    };
    static constexpr std::string_view description = "dense set (a bitset over indices)";
    static constexpr std::string_view holds = "U bits";
};
template<typename K>
struct declaration_of<repr::DenseSet<K, false>>
{
    using declaration = dense_set_decl;
    static constexpr property_set roles = element_roles<K>();
};

}  // namespace detail

namespace detail
{

/// dense set tracking its words (a bitset over indices, cleared by the words it touched)
struct tracked_dense_set_decl
{
    static constexpr std::string_view id = "tracked_dense_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/dense_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ order::sorted, uniqueness::unique, keys::dense, representation::canonical, memory::retains, clearing::proportional_clear });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index>(), traits<role::order, trait::own_order>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost ordered_lookup = costs::lin_keys;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::lin_n;
        static constexpr cost iterate = costs::lin_keys;
        static constexpr double words = 0.03;
    };
    static constexpr std::string_view description = "dense set tracking its words (a bitset over indices, cleared by the words it touched)";
    static constexpr std::string_view holds = "U bits + the indices of the words in use";
};
template<typename K>
struct declaration_of<repr::DenseSet<K, true>>
{
    using declaration = tracked_dense_set_decl;
    static constexpr property_set roles = element_roles<K>();
};

}  // namespace detail

namespace detail
{

/// sparse set (members array plus index positions, O(1) clear)
struct sparse_set_decl
{
    static constexpr std::string_view id = "sparse_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/sparse_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides =
        property_set::of({ uniqueness::unique, keys::dense, position::positional, layout::contiguous, memory::retains, clearing::constant_clear });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index, trait::destroys_trivially>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost erase = costs::one;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_n;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "sparse set (members array plus index positions, O(1) clear)";
    static constexpr std::string_view holds = "n members + U positions";
};
template<typename K>
struct declaration_of<repr::SparseSet<K, false>>
{
    using declaration = sparse_set_decl;
    static constexpr property_set roles = element_roles<K>();
};

}  // namespace detail

namespace detail
{

/// sparse set keeping insertion order (erase shifts the members after it)
struct ordered_sparse_set_decl
{
    static constexpr std::string_view id = "ordered_sparse_set";
    static constexpr auto headers = header_list<1> { "urd/repr/values/sparse_set.hpp" };
    using kind = set_kind;
    static constexpr property_set provides = property_set::of(
        { uniqueness::unique, keys::dense, position::positional, layout::contiguous, memory::retains, clearing::constant_clear, order::insertion_ordered });
    static constexpr property_set relies_on = relies(traits<role::element, trait::an_index, trait::destroys_trivially>());
    static constexpr std::array<tag, 1> gives_elements { memory::reclaims };
    struct operation_costs
    {
        static constexpr cost insert = costs::one_a;
        static constexpr cost lookup = costs::one;
        static constexpr cost erase = costs::lin_n;
        static constexpr cost clear = costs::one;
        static constexpr cost iterate = costs::lin_n;
        static constexpr cost access = costs::one;
        static constexpr double words = 2;
    };
    static constexpr std::string_view description = "sparse set keeping insertion order (erase shifts the members after it)";
    static constexpr std::string_view holds = "n members in insertion order + U positions";
};
template<typename K>
struct declaration_of<repr::SparseSet<K, true>>
{
    using declaration = ordered_sparse_set_decl;
    static constexpr property_set roles = element_roles<K>();
};

}  // namespace detail

}  // namespace urd
