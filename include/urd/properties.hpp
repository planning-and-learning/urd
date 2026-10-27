#pragma once

#include "urd/traits.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace urd
{

// What a Store can be asked for: element types, the property tags of traits.hpp, and a workload. Each is a
// type, so they can be listed together, in any order.

/// The elements are identified by keys of type K. Without value<V>, the keys are the contents: a set.
template<typename K>
struct key
{
    using type = K;
};

/// Each element carries a value of type V. Without key<K>, values are identified by position: a sequence.
template<typename V>
struct value
{
    using type = V;
};

/// How a Store is used. It does not change what a Store guarantees, only which representation is chosen.
enum class workload : std::uint8_t
{
    general,
    many_similar,   ///< many values that differ little, such as search states
    dense_mutable,  ///< one large value changed in place
    small,          ///< a few elements
    lookup_heavy,   ///< mostly membership tests and lookups
};

struct many_similar
{
};
struct dense_mutable
{
};
struct small
{
};
struct lookup_heavy
{
};

namespace detail
{

template<typename T>
struct workload_of
{
    static constexpr bool is_workload = false;
    static constexpr workload value = workload::general;
};
template<>
struct workload_of<many_similar>
{
    static constexpr bool is_workload = true;
    static constexpr workload value = workload::many_similar;
};
template<>
struct workload_of<dense_mutable>
{
    static constexpr bool is_workload = true;
    static constexpr workload value = workload::dense_mutable;
};
template<>
struct workload_of<small>
{
    static constexpr bool is_workload = true;
    static constexpr workload value = workload::small;
};
template<>
struct workload_of<lookup_heavy>
{
    static constexpr bool is_workload = true;
    static constexpr workload value = workload::lookup_heavy;
};

template<typename T>
inline constexpr bool is_key = false;
template<typename K>
inline constexpr bool is_key<key<K>> = true;

template<typename T>
inline constexpr bool is_value = false;
template<typename V>
inline constexpr bool is_value<value<V>> = true;

/// A tag a Store may be asked for. Mutability is not one: it follows from frozen and monotone.
template<typename T>
inline constexpr bool is_tag = (is_guarantee<T> && !std::derived_from<T, immutable>) || is_request_only<T>;

template<typename T>
inline constexpr bool is_property = is_tag<T> || workload_of<T>::is_workload || is_key<T> || is_value<T>;

/// The type in the first key<…> of Ps, else void.
template<typename... Ps>
struct key_type_of
{
    using type = void;
};
template<typename K, typename... Ps>
struct key_type_of<key<K>, Ps...>
{
    using type = K;
};
template<typename P, typename... Ps>
struct key_type_of<P, Ps...> : key_type_of<Ps...>
{
};

/// The type in the first value<…> of Ps, else void.
template<typename... Ps>
struct value_type_of
{
    using type = void;
};
template<typename V, typename... Ps>
struct value_type_of<value<V>, Ps...>
{
    using type = V;
};
template<typename P, typename... Ps>
struct value_type_of<P, Ps...> : value_type_of<Ps...>
{
};

}  // namespace detail

/// A Store's request in canonical form: whatever order the properties were listed in, equal requests give
/// equal specs, and so equal Store types. Its categories are those of Properties.
template<typename K, typename V, typename Properties, workload Work>
struct spec : Properties
{
    using key_type = K;
    using value_type = V;
    static constexpr workload work = Work;

    static constexpr bool is_set = !std::is_void_v<K> && std::is_void_v<V>;
    static constexpr bool is_map = !std::is_void_v<K> && !std::is_void_v<V>;
    static constexpr bool is_sequence = std::is_void_v<K> && !std::is_void_v<V>;

    /// What a key maps to, in a map.
    using mapped_type = std::conditional_t<is_map, V, void>;
    /// What a Store's elements are: keys in a set, values in a sequence, key-value pairs in a map.
    using element_type = std::conditional_t<is_set, K, std::conditional_t<is_map, std::pair<const K, V>, V>>;
};

namespace detail
{

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...),
                  "urd::Store: every argument must be an urd property (key<K>, value<V>, "
                  "sorted, unique, … or a workload such as small)");
    static_assert((0 + ... + int(is_key<Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + int(is_value<Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert((0 + ... + int(workload_of<Ps>::is_workload)) <= 1, "urd::Store: at most one workload");
    static_assert(one_per_category<guaranteed_roots, Ps...> && one_per_category<requested_roots, Ps...>,
                  "urd::Store: two properties answer the same question (dense and sparse, functional and multi, "
                  "frozen and monotone, …)");

    using key_type = typename key_type_of<Ps...>::type;
    using value_type = typename value_type_of<Ps...>::type;
    static constexpr bool has_key = !std::is_void_v<key_type>;
    static constexpr bool has_value = !std::is_void_v<value_type>;
    static_assert(has_key || has_value, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    using stated = properties_of<Ps...>;
    static_assert(!(has_key && has<stated, positional>), "urd::Store: positional identifies elements by position, so it cannot have key<K>");
    static_assert(!has<stated, contiguous, stable_refs> || has<stated, frozen>,
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");

    // What the list implies.
    static constexpr bool is_sequence = has_value && !has_key;
    static constexpr bool is_set = has_key && !has_value;
    using identity = std::conditional_t<is_sequence, positional, by_key>;
    // A set's keys are its contents, each once.
    // A map's keys are each once, unless it is a multimap.
    using uniqueness = std::conditional_t<is_set || (has_key && has_value && !has<stated, multi>), unique, typename stated::uniqueness_category>;
    // A map has one value per key unless it says multi; a sequence one value per position.
    using multiplicity = std::conditional_t<(has_key && has_value && !has<stated, multi>) || is_sequence, functional, typename stated::multiplicity_category>;
    // Positions are 0..n-1.
    using keys = std::conditional_t<is_sequence, dense, typename stated::key_category>;
    // Nothing moves once nothing changes.
    using references = std::conditional_t<has<stated, frozen>, stable_refs, typename stated::reference_category>;
    // What the representation must offer: nothing if frozen, inserts if it only grows or must stay nonempty.
    using mutability =
        std::conditional_t<has<stated, frozen>, immutable, std::conditional_t<has<stated, monotone> || has<stated, nonempty>, growable, shrinkable>>;

    static constexpr workload work = []
    {
        auto found = workload::general;
        ((found = workload_of<Ps>::is_workload ? workload_of<Ps>::value : found), ...);
        return found;
    }();

    using type = spec<key_type,
                      value_type,
                      properties<typename stated::order_category,
                                 uniqueness,
                                 keys,
                                 references,
                                 typename stated::layout_category,
                                 typename stated::canonical_category,
                                 typename stated::concurrency_category,
                                 typename stated::version_category,
                                 mutability,
                                 typename stated::lifecycle_category,
                                 typename stated::emptiness_category,
                                 multiplicity,
                                 identity,
                                 typename stated::memory_category>,
                      work>;
};

}  // namespace detail

}  // namespace urd
