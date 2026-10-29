#pragma once

#include "urd/store/types.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace urd::detail
{

// A Store offers the operations its properties allow and no others (store.hpp). Where a facet is off, its names
// would be missing, and a call would fail with "no member named insert" and no word on why. The denials below stand
// in for the missing facets: each declares the same names as function templates constrained by a concept that is
// never satisfied and carries the reason as its argument, so the error names it ("insert: the Store is frozen; …"),
// while `requires { store.insert(x); }` stays false, as for a missing member. A denial is inherited only where no
// facet of the Store declares any of its names, since two bases declaring one name would make its lookup ambiguous;
// the tables below say which facets declare which names, and tests/denials.cpp checks them against the facets.

/// A string literal as a template argument: the reason a denied operation is denied.
template<std::size_t N>
struct fixed_string
{
    std::array<char, N> text {};

    consteval fixed_string(const char (&literal)[N]) { std::copy_n(literal, N, text.begin()); }
};

/// Never satisfied: a member constrained by it exists for the diagnostic alone.
template<fixed_string Reason>
concept denied = false;

template<bool Condition, typename Then, typename Else>
using if_t = std::conditional_t<Condition, Then, Else>;

// The denials, one class per group of names that the same facets declare.

template<fixed_string Reason>
struct no_insert
{
    template<typename... Args>
        requires denied<Reason>
    void insert(Args&&...);
};
template<fixed_string Reason>
struct no_emplace
{
    template<typename... Args>
        requires denied<Reason>
    void emplace(Args&&...);
};
template<fixed_string Reason>
struct no_appending
{
    template<typename... Args>
        requires denied<Reason>
    void push_back(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void emplace_back(Args&&...);
};
template<fixed_string Reason>
struct no_pop_back
{
    template<typename... Args>
        requires denied<Reason>
    void pop_back(Args&&...);
};
template<fixed_string Reason>
struct no_erase
{
    template<typename... Args>
        requires denied<Reason>
    void erase(Args&&...);
};
template<fixed_string Reason>
struct no_clear
{
    template<typename... Args>
        requires denied<Reason>
    void clear(Args&&...);
};
template<fixed_string Reason>
struct no_lookup
{
    template<typename... Args>
        requires denied<Reason>
    void find(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void count(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void contains(Args&&...) const;
};
template<fixed_string Reason>
struct no_ordered_lookup
{
    template<typename... Args>
        requires denied<Reason>
    void lower_bound(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void upper_bound(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void equal_range(Args&&...) const;
};
template<fixed_string Reason>
struct no_iteration
{
    template<typename... Args>
        requires denied<Reason>
    void begin(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void end(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void cbegin(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void cend(Args&&...) const;
};
template<fixed_string Reason>
struct no_reverse_iteration
{
    template<typename... Args>
        requires denied<Reason>
    void rbegin(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void rend(Args&&...) const;
};
template<fixed_string Reason>
struct no_subscript
{
    template<typename Arg>
        requires denied<Reason>
    void operator[](Arg&&) const;
    template<typename... Args>
        requires denied<Reason>
    void at(Args&&...) const;
};
template<fixed_string Reason>
struct no_ends
{
    template<typename... Args>
        requires denied<Reason>
    void front(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void back(Args&&...) const;
};
template<fixed_string Reason>
struct no_position
{
    template<typename... Args>
        requires denied<Reason>
    void position(Args&&...) const;
};
template<fixed_string Reason>
struct no_data
{
    template<typename... Args>
        requires denied<Reason>
    void data(Args&&...) const;
};
template<fixed_string Reason>
struct no_reserve
{
    template<typename... Args>
        requires denied<Reason>
    void reserve(Args&&...);
};
template<fixed_string Reason>
struct no_sizing
{
    template<typename... Args>
        requires denied<Reason>
    void size(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void empty(Args&&...) const;
};
template<fixed_string Reason>
struct no_values_by_key
{
    template<typename... Args>
        requires denied<Reason>
    void try_emplace(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void insert_or_assign(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void read(Args&&...) const;
};
template<fixed_string Reason>
struct no_update
{
    template<typename... Args>
        requires denied<Reason>
    void update(Args&&...);
};
template<fixed_string Reason>
struct no_merge
{
    template<typename... Args>
        requires denied<Reason>
    void merge(Args&&...);
};
template<fixed_string Reason>
struct no_values
{
    template<typename... Args>
        requires denied<Reason>
    void values(Args&&...) const;
};
template<fixed_string Reason>
struct no_keys
{
    template<typename... Args>
        requires denied<Reason>
    void keys(Args&&...) const;
};
template<fixed_string Reason>
struct no_queue
{
    template<typename... Args>
        requires denied<Reason>
    void push(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void pop(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void top(Args&&...) const;
};
template<fixed_string Reason>
struct no_scopes
{
    template<typename... Args>
        requires denied<Reason>
    void push_scope(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void pop_scope(Args&&...);
    template<typename... Args>
        requires denied<Reason>
    void depth(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void bound_in_scope(Args&&...) const;
};
template<fixed_string Reason>
struct no_layers
{
    template<typename... Args>
        requires denied<Reason>
    void layer_over(Args&&...);
};
template<fixed_string Reason>
struct no_changes
{
    template<typename... Args>
        requires denied<Reason>
    void added(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void removed(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void clear_changes(Args&&...);
};
template<fixed_string Reason>
struct no_view
{
    template<typename... Args>
        requires denied<Reason>
    void view(Args&&...) const;
};
template<fixed_string Reason>
struct no_rows
{
    template<typename... Args>
        requires denied<Reason>
    void get(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void row_size(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void memory_usage(Args&&...) const;
};
template<fixed_string Reason>
struct no_acquire
{
    template<typename... Args>
        requires denied<Reason>
    void acquire(Args&&...);
};
template<fixed_string Reason>
struct no_grouping
{
    template<typename... Args>
        requires denied<Reason>
    void same(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void class_size(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void classes(Args&&...) const;
};
template<fixed_string Reason>
struct no_reset
{
    template<typename... Args>
        requires denied<Reason>
    void reset(Args&&...);
};
template<fixed_string Reason>
struct no_set_algebra
{
    template<typename... Args>
        requires denied<Reason>
    void includes(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void intersects(Args&&...) const;
    template<typename... Args>
        requires denied<Reason>
    void unite(Args&&...);
};

/// Which facets declare each group of names: where any of them is on, the group's denial stays off. tests/denials.cpp
/// checks these against the facets, so a facet that gains or loses a name is caught.
inline constexpr std::array declares_insert { facet_id::growth, facet_id::pool, facet_id::records, facet_id::slots, facet_id::grouping };
inline constexpr std::array declares_emplace { facet_id::growth, facet_id::queue_access, facet_id::slots };
inline constexpr std::array declares_appending { facet_id::growth };
inline constexpr std::array declares_pop_back { facet_id::removal };
inline constexpr std::array declares_erase { facet_id::removal, facet_id::queue_access, facet_id::slots };
inline constexpr std::array declares_clear {
    facet_id::removal, facet_id::queue_access, facet_id::pool, facet_id::records, facet_id::slots, facet_id::grouping
};
inline constexpr std::array declares_lookup { facet_id::lookup, facet_id::grouping, facet_id::pool, facet_id::records };
inline constexpr std::array declares_ordered_lookup { facet_id::ordered_lookup };
inline constexpr std::array declares_iteration { facet_id::iteration };
inline constexpr std::array declares_reverse_iteration { facet_id::reverse_iteration };
inline constexpr std::array declares_subscript { facet_id::positions, facet_id::values_by_key, facet_id::slots, facet_id::records };
inline constexpr std::array declares_ends { facet_id::positions, facet_id::queue_access };
inline constexpr std::array declares_position { facet_id::positions, facet_id::entries };
inline constexpr std::array declares_data { facet_id::contiguous_data };
inline constexpr std::array declares_reserve { facet_id::capacity };
inline constexpr std::array declares_sizing { facet_id::sizing };
inline constexpr std::array declares_values_by_key { facet_id::values_by_key, facet_id::pool };
inline constexpr std::array declares_update { facet_id::values_by_key, facet_id::queue_access };
inline constexpr std::array declares_merge { facet_id::values_by_key, facet_id::grouping };
inline constexpr std::array declares_values { facet_id::multi_values, facet_id::entries };
inline constexpr std::array declares_keys { facet_id::inverse, facet_id::entries };
inline constexpr std::array declares_queue { facet_id::queue_access };
inline constexpr std::array declares_scopes { facet_id::scopes };
inline constexpr std::array declares_layers { facet_id::layers };
inline constexpr std::array declares_changes { facet_id::changes };
inline constexpr std::array declares_view { facet_id::pool, facet_id::positions, facet_id::records };
inline constexpr std::array declares_rows { facet_id::pool, facet_id::records };
inline constexpr std::array declares_acquire { facet_id::slots };
inline constexpr std::array declares_grouping { facet_id::grouping };
inline constexpr std::array declares_reset { facet_id::shape };
inline constexpr std::array declares_set_algebra { facet_id::set_algebra };

/// The denials a Store for spec S over R inherits: for each group of names no facet of it declares, the reason that
/// fits its properties.
template<typename R, typename S>
struct denial
{
    using on = facet_switches<R, S>;
    static constexpr bool is_frozen = has<S, frozen>;
    static constexpr bool is_fixed = has<S, fixed_shape>;
    static constexpr bool is_monotone = has<S, monotone>;
    static constexpr bool is_keyed = by_value<S>;
    static constexpr bool is_multi = S::is_map && !has<S, unique>;

    using insert = facet_if<!on::any_on(declares_insert),
                            if_t<is_frozen,
                                 no_insert<"insert: the Store is frozen; it changes only by assign(first, last)">,
                                 if_t<is_fixed,
                                      no_insert<"insert: fixed_shape rows are set at construction; their contents change in place, reset() clears them">,
                                      if_t<S::is_queue, no_insert<"insert: a queue takes push">, no_insert<"insert: not offered by this kind of Store">>>>>;
    using emplace = facet_if<!on::any_on(declares_emplace),
                             if_t<is_frozen,
                                  no_emplace<"emplace: the Store is frozen; it changes only by assign(first, last)">,
                                  if_t<is_fixed,
                                       no_emplace<"emplace: fixed_shape rows are set at construction; their contents change in place">,
                                       no_emplace<"emplace: not offered by this kind of Store">>>>;
    using appending = facet_if<!on::any_on(declares_appending),
                               if_t<is_keyed,
                                    no_appending<"push_back/emplace_back: a keyed Store (a set or a map) takes insert">,
                                    if_t<is_frozen,
                                         no_appending<"push_back/emplace_back: the Store is frozen; it changes only by assign(first, last)">,
                                         if_t<S::is_queue,
                                              no_appending<"push_back/emplace_back: a queue takes push">,
                                              if_t<S::is_pool,
                                                   no_appending<"push_back/emplace_back: a pool takes insert, which gives a handle">,
                                                   no_appending<"push_back/emplace_back: not offered by this kind of Store">>>>>>;
    using pop_back =
        facet_if<!on::any_on(declares_pop_back),
                 if_t<is_keyed,
                      no_pop_back<"pop_back: a keyed Store (a set or a map) erases by key">,
                      if_t<is_monotone,
                           no_pop_back<"pop_back: the Store is monotone, it only grows; drop monotone">,
                           if_t<is_frozen,
                                no_pop_back<"pop_back: the Store is frozen; it changes only by assign(first, last)">,
                                if_t<S::is_queue, no_pop_back<"pop_back: a queue takes pop">, no_pop_back<"pop_back: not offered by this kind of Store">>>>>>;
    using erase = facet_if<!on::any_on(declares_erase),
                           if_t<is_frozen,
                                no_erase<"erase: the Store is frozen; it changes only by assign(first, last)">,
                                if_t<is_monotone,
                                     no_erase<"erase: the Store is monotone, it only grows; drop monotone">,
                                     if_t<is_fixed,
                                          no_erase<"erase: fixed_shape rows keep their shape; reset() clears their contents">,
                                          if_t<S::is_pool,
                                               no_erase<"erase: an interning pool keeps its rows; nothing erases them">,
                                               no_erase<"erase: not offered by this kind of Store">>>>>>;
    using clear = facet_if<!on::any_on(declares_clear),
                           if_t<is_frozen,
                                no_clear<"clear: the Store is frozen; it changes only by assign(first, last)">,
                                if_t<is_monotone,
                                     no_clear<"clear: the Store is monotone, it only grows; drop monotone">,
                                     if_t<is_fixed,
                                          no_clear<"clear: fixed_shape rows keep their shape; reset() clears their contents">,
                                          no_clear<"clear: not offered by this kind of Store">>>>>;
    using lookup = facet_if<!on::any_on(declares_lookup),
                            if_t<S::is_queue,
                                 no_lookup<"find/count/contains: a queue is reached by its front or top only">,
                                 if_t<S::is_sequence,
                                      no_lookup<"find/count/contains: a sequence is reached by position; a set or a map looks keys up">,
                                      no_lookup<"find/count/contains: not offered by this kind of Store">>>>;
    using ordered_lookup = facet_if<!on::any_on(declares_ordered_lookup),
                                    if_t<has<S, sorted>,
                                         no_ordered_lookup<"lower_bound/upper_bound/equal_range: a map ordered by its values is found by key only">,
                                         no_ordered_lookup<"lower_bound/upper_bound/equal_range: the Store is not sorted; ask for sorted">>>;
    using iteration = facet_if<!on::any_on(declares_iteration),
                               if_t<S::is_queue,
                                    no_iteration<"begin/end: a queue is reached by its front or top, not iterated">,
                                    if_t<S::is_pool,
                                         no_iteration<"begin/end: a pool is reached by handle (get, view), not iterated">,
                                         if_t<S::is_partition,
                                              no_iteration<"begin/end: a partition answers find, same and classes, and is not iterated">,
                                              no_iteration<"begin/end: not offered by this kind of Store">>>>>;
    using reverse_iteration = facet_if<!on::any_on(declares_reverse_iteration),
                                       if_t<S::iterable,
                                            no_reverse_iteration<"rbegin/rend: the Store has no order to reverse; ask for sorted or insertion_ordered">,
                                            no_reverse_iteration<"rbegin/rend: the Store is not iterated">>>;
    using subscript = facet_if<!on::any_on(declares_subscript),
                               if_t<S::is_set,
                                    no_subscript<"operator[]/at: a set has no positions; ask for positional">,
                                    if_t<is_multi,
                                         no_subscript<"operator[]/at: a multimap has many values per key; use values(key)">,
                                         if_t<S::is_queue,
                                              no_subscript<"operator[]/at: a queue is reached by its front or top only">,
                                              if_t<S::is_pool,
                                                   no_subscript<"operator[]/at: an interning pool reads rows by handle: get(h), view(h)">,
                                                   no_subscript<"operator[]/at: not offered by this kind of Store">>>>>>;
    using ends = facet_if<!on::any_on(declares_ends),
                          if_t<S::is_set,
                               no_ends<"front/back: a set has no positions; ask for positional">,
                               if_t<S::is_map, no_ends<"front/back: a map is reached by key">, no_ends<"front/back: not offered by this kind of Store">>>>;
    using position =
        facet_if<!on::any_on(declares_position), no_position<"position: the Store has no positions; ask for positional (a set) or insertion_ordered (a map)">>;
    using data = facet_if<!on::any_on(declares_data), no_data<"data: the Store's elements are not contiguous; ask for contiguous">>;
    using reserve = facet_if<!on::any_on(declares_reserve), no_reserve<"reserve: the Store does not grow (frozen, or fixed_shape)">>;
    using sizing = facet_if<!on::any_on(declares_sizing), no_sizing<"size/empty: an interning pool shares parts of its rows and keeps no count of them">>;
    using values_by_key = facet_if<!on::any_on(declares_values_by_key),
                                   if_t<is_multi,
                                        no_values_by_key<"try_emplace/insert_or_assign/read: a multimap has many values per key; insert and values(key)">,
                                        no_values_by_key<"try_emplace/insert_or_assign/read: only a map with one value per key">>>;
    using update = facet_if<!on::any_on(declares_update),
                            no_update<"update: a map with one value per key changes a value in place; a priority queue with handles an element">>;
    using merge = facet_if<!on::any_on(declares_merge), no_merge<"merge: a map with one value per key merges values; a partition merges classes">>;
    using values = facet_if<!on::any_on(declares_values), no_values<"values: a multimap's values by key, or an insertion-ordered map's values in order">>;
    using keys = facet_if<!on::any_on(declares_keys), no_keys<"keys: a bidirectional map's keys by value, or an insertion-ordered map's keys in order">>;
    using queue = facet_if<!on::any_on(declares_queue),
                           if_t<S::is_sequence,
                                no_queue<"push/pop/top: a sequence takes push_back; a queue asks for fifo, lifo or by_priority">,
                                no_queue<"push/pop/top: not a queue; ask for fifo, lifo or by_priority">>>;
    using scopes = facet_if<!on::any_on(declares_scopes), no_scopes<"push_scope/pop_scope/depth/bound_in_scope: ask for scoped">>;
    using layers = facet_if<!on::any_on(declares_layers), no_layers<"layer_over: ask for layered">>;
    using changes = facet_if<!on::any_on(declares_changes), no_changes<"added/removed/clear_changes: ask for change_log">>;
    using view = facet_if<!on::any_on(declares_view), no_view<"view: a pool lends a row by handle, a positional set an element by index">>;
    using rows = facet_if<!on::any_on(declares_rows), no_rows<"get/row_size/memory_usage: only a pool of interned rows (handles, unique)">>;
    using acquire = facet_if<!on::any_on(declares_acquire), no_acquire<"acquire: only a pool of slots (handles without unique)">>;
    using grouping = facet_if<!on::any_on(declares_grouping), no_grouping<"same/class_size/classes: ask for partition">>;
    using reset = facet_if<!on::any_on(declares_reset), no_reset<"reset: ask for fixed_shape">>;
    using set_algebra = facet_if<!on::any_on(declares_set_algebra), no_set_algebra<"includes/intersects/unite: only a set (key<K> without value<V>)">>;
};

}  // namespace urd::detail
