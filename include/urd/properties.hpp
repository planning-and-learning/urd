#pragma once

#include "urd/props.hpp"

#include <cstddef>
#include <type_traits>

namespace urd
{

// The properties a Store can be asked for. Each is a type, so element types (key<K>, value<V>) and flags
// can be listed together, in any order.

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

struct sorted
{
};
struct unique
{
};
struct nonempty
{
};
struct positional
{
};
struct functional
{
};
struct multi
{
};
struct dense
{
};
struct sparse
{
};
struct frozen
{
};
struct monotone
{
};
struct stable_refs
{
};
struct persistent
{
};
struct contiguous
{
};
struct canonical
{
};

/// How a Store is used. It does not change what a Store guarantees, only which representation is chosen.
enum class workload
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
struct flag_of
{
    static constexpr props value = props::none;
};
template<>
struct flag_of<sorted>
{
    static constexpr props value = props::sorted;
};
template<>
struct flag_of<unique>
{
    static constexpr props value = props::unique;
};
template<>
struct flag_of<nonempty>
{
    static constexpr props value = props::nonempty;
};
template<>
struct flag_of<positional>
{
    static constexpr props value = props::positional;
};
template<>
struct flag_of<functional>
{
    static constexpr props value = props::functional;
};
template<>
struct flag_of<multi>
{
    static constexpr props value = props::multi;
};
template<>
struct flag_of<dense>
{
    static constexpr props value = props::dense;
};
template<>
struct flag_of<sparse>
{
    static constexpr props value = props::sparse;
};
template<>
struct flag_of<frozen>
{
    static constexpr props value = props::frozen;
};
template<>
struct flag_of<monotone>
{
    static constexpr props value = props::monotone;
};
template<>
struct flag_of<stable_refs>
{
    static constexpr props value = props::stable_refs;
};
template<>
struct flag_of<persistent>
{
    static constexpr props value = props::persistent;
};
template<>
struct flag_of<contiguous>
{
    static constexpr props value = props::contiguous;
};
template<>
struct flag_of<canonical>
{
    static constexpr props value = props::canonical;
};

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

template<typename T>
inline constexpr bool is_property =
    flag_of<T>::value != props::none || workload_of<T>::is_workload || is_key<T> || is_value<T>;

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

/// The properties a list states and the ones they imply.
[[nodiscard]] constexpr props with_implications(props flags, bool has_key, bool has_value) noexcept
{
    if (has_value && !has_key)
        flags = flags | props::positional;  // values without keys are a sequence
    if (has_key && !has_value)
        flags = flags | props::unique;  // a set's keys are its contents, each once
    if (has_key && has_value && !includes(flags, props::multi))
        flags = flags | props::functional;  // a map has one value per key unless it says multi
    if (includes(flags, props::positional))
        flags = flags | props::functional | props::dense;  // one value per position, positions 0..n-1
    if (includes(flags, props::frozen))
        flags = flags | props::stable_refs;  // nothing moves once nothing changes
    return flags;
}

}  // namespace detail

/// A Store's request in canonical form: whatever order the properties were listed in, equal requests give
/// equal specs, and so equal Store types.
template<typename K, typename V, props Flags, workload Work>
struct spec
{
    using key_type = K;
    using value_type = V;
    static constexpr props flags = Flags;
    static constexpr workload work = Work;

    static constexpr bool is_set = !std::is_void_v<K> && std::is_void_v<V>;
    static constexpr bool is_map = !std::is_void_v<K> && !std::is_void_v<V>;
    static constexpr bool is_sequence = std::is_void_v<K> && !std::is_void_v<V>;

    /// What a Store's elements are: keys in a set, values in a sequence.
    using element_type = std::conditional_t<is_set, K, V>;
};

namespace detail
{

/// Checks a property list and turns it into its spec. Each static_assert names what is wrong.
template<typename... Ps>
struct to_spec
{
    static_assert((is_property<Ps> && ...), "urd::Store: every argument must be an urd property (key<K>, value<V>, "
                                            "sorted, unique, … or a workload such as small)");
    static_assert((0 + ... + int(is_key<Ps>)) <= 1, "urd::Store: at most one key<K>");
    static_assert((0 + ... + int(is_value<Ps>)) <= 1, "urd::Store: at most one value<V>");
    static_assert((0 + ... + int(workload_of<Ps>::is_workload)) <= 1, "urd::Store: at most one workload");

    using key_type = typename key_type_of<Ps...>::type;
    using value_type = typename value_type_of<Ps...>::type;
    static constexpr bool has_key = !std::is_void_v<key_type>;
    static constexpr bool has_value = !std::is_void_v<value_type>;
    static_assert(has_key || has_value, "urd::Store: needs key<K> (a set or map) or value<V> (a sequence)");

    static constexpr props stated = (props::none | ... | flag_of<Ps>::value);
    static_assert(!(has_key && includes(stated, props::positional)),
                  "urd::Store: positional identifies elements by position, so it cannot have key<K>");
    static_assert(!includes(stated, props::functional | props::multi),
                  "urd::Store: functional (one value per key) and multi (many) exclude each other");
    static_assert(!includes(stated, props::dense | props::sparse), "urd::Store: dense and sparse exclude each other");
    static_assert(!includes(stated, props::frozen | props::monotone),
                  "urd::Store: frozen (never changes) and monotone (grows) exclude each other");
    static_assert(!(includes(stated, props::contiguous | props::stable_refs) && !includes(stated, props::frozen)),
                  "urd::Store: contiguous storage moves its elements when it grows, so stable_refs needs frozen too");

    static constexpr workload work = []
    {
        auto found = workload::general;
        ((found = workload_of<Ps>::is_workload ? workload_of<Ps>::value : found), ...);
        return found;
    }();

    using type = spec<key_type, value_type, with_implications(stated, has_key, has_value), work>;
};

}  // namespace detail

}  // namespace urd
