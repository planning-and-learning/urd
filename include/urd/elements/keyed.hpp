#pragma once
// The record of a key in nested containers: a key<P..., T> addresses its record level by level, the first part
// selecting a container (a map's value), the next looking up in that, and so on, the last part numbering the record in
// the innermost container (a pool of data<T> handing out key<T>); a family of stores (one per record type) first
// selects the store of the key's kind. One fold for any number of parts and any store: an index, key<T>, is its
// store's own lookup. A context's operator[](key) folds through here.
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"

#include <boost/mp11.hpp>
#include <concepts>
#include <cstddef>
#include <utility>

namespace urd
{

/// Whether a context C hands out records of T by handles H: `context[handle]` is what stored_ref<T> is made from (a
/// record held in place, a reader of a record packed in its slot, or the stored_ref itself). A handle is an index, a key,
/// or one of the context's own shape.
template<typename C, typename T, typename H>
concept HandsOutRecordsBy = requires(const C& context, const H& handle) { stored_ref<T>(context[handle]); };

/// Whether S hands out records of T by their index (HandsOutRecordsBy): a container, or any context of views that
/// reads records by index.
template<typename S, typename T>
concept HandsOutRecordsOf = HandsOutRecordsBy<S, T, key<T>>;

/// Whether S holds records of T by their index (HandsOutRecordsOf), and `store.size()` is the number of indices along
/// its layers (an index below it is held). The innermost container of a nest (a pool of data<T>).
template<typename S, typename T>
concept HoldsRecordsOf = HandsOutRecordsOf<S, T> && requires(const S& store) {
    { store.size() } -> std::convertible_to<std::size_t>;
};

/// Whether S answers itself whether it holds a key of T (`store.contains(key)`): a store whose keys are no numbers
/// (key_value_of: a pool's handle), which no size bounds.
template<typename S, typename T>
concept ContainsKeysOf = requires(const S& store, key<T> at) {
    { store.contains(at) } -> std::convertible_to<bool>;
};

/// Whether S selects a container by an index of P: `store.at(key<P>)` is the container the index selects, from
/// whichever layer holds it (std::out_of_range where none does), and `store.visit(key<P>, f, missing)` is f of it,
/// or `missing` (a layered map of containers: urd::layered).
template<typename S, typename P>
concept SelectsBy = requires(const S& store, key<P> at) {
    store.at(at);
    {
        store.visit(at, [](const auto& /*inner*/) { return true; }, false)
    } -> std::same_as<bool>;
};

/// Whether S is a family of stores that selects the store of T's records first: `store.template get<T>()` is a store
/// that holds records of T (HoldsRecordsOf), the step a key of T takes before its fold (a symbol repository: one pool per
/// record type).
template<typename S, typename T>
concept SelectsStoreOf = requires(const S& store) {
    { store.template get<T>() } -> HoldsRecordsOf<T>;
};

/// Whether a context C hands out the record of a key K itself, as its stored_ref (a repository: context[key]).
template<typename C, typename K>
concept HandsOutStoredRefs = requires(const C& context, K at) {
    { context[at] } -> std::same_as<stored_ref<typename K::element_type>>;
};

namespace detail
{
/// The first part of the key K, and the key without it.
template<typename K>
using first_part = boost::mp11::mp_front<boost::mp11::mp_rename<K, boost::mp11::mp_list>>;

template<typename K>
using rest_t = boost::mp11::mp_rename<boost::mp11::mp_pop_front<boost::mp11::mp_rename<K, boost::mp11::mp_list>>, key>;

template<typename P, typename... Ts>
[[nodiscard]] key<Ts...> rest_of(key<P, Ts...> at) noexcept
{ return key<Ts...>(urd::get<Ts>(at)...); }

/// The key of the parts `known` followed by the parts of `rest`.
template<typename... Ps, typename... Ts>
[[nodiscard]] key<Ps..., Ts...> joined_key(key<Ps...> known, key<Ts...> rest) noexcept
{ return key<Ps..., Ts...>(urd::get<Ps>(known)..., urd::get<Ts>(rest)...); }
}  // namespace detail

/// The record of a key, handed out by value as its stored_ref, one step at a time: an index in a store that holds its
/// records, the stored_ref made from the store's (store[at]: always the store's own step); from a context that hands
/// the record of the whole key out itself (HandsOutStoredRefs: a repository); a family first selects the store of the
/// key's kind (store.get<T>()); a key of several parts selects a container by its first part (store.at(first)) and
/// folds the rest there. std::out_of_range where a selection finds nothing; an index no layer holds is not looked for
/// (contains_key).
template<typename S, typename... Ts>
[[nodiscard]] stored_ref<typename key<Ts...>::element_type> record_at(const S& store, key<Ts...> at)
{
    using T = typename key<Ts...>::element_type;
    if constexpr (sizeof...(Ts) == 1 && HandsOutRecordsOf<S, T>)
        return stored_ref<T>(store[at]);
    else if constexpr (HandsOutStoredRefs<S, key<Ts...>>)
        return store[at];
    else if constexpr (SelectsStoreOf<S, T>)
        return urd::record_at(store.template get<T>(), at);
    else if constexpr (sizeof...(Ts) > 1 && SelectsBy<S, detail::first_part<key<Ts...>>>)
        return urd::record_at(store.at(urd::get<detail::first_part<key<Ts...>>>(at)), detail::rest_of(at));
    else
        static_assert(false, "urd::record_at: a store that holds the key's records, selects their store or selects by the key's first part");
}

/// The record of T for a handle of any shape its context hands records out by (a view's handle): stored_ref<T> made
/// from context[handle]. T is named, as a handle of the context's own shape does not say it.
template<typename T, typename C, typename H>
    requires HandsOutRecordsBy<C, T, H>
[[nodiscard]] stored_ref<T> record_at(const C& context, const H& handle)
{ return stored_ref<T>(context[handle]); }

/// Whether some layer holds the key, by the same steps: an index below its store's size, a key of another value as its
/// store answers; in a family, in the store of
/// its kind; a key of several parts, where its first part selects a container (store.visit) that holds the rest.
template<typename S, typename... Ts>
[[nodiscard]] bool contains_key(const S& store, key<Ts...> at)
{
    using T = typename key<Ts...>::element_type;
    if constexpr (sizeof...(Ts) == 1 && NumberedKind<T> && HoldsRecordsOf<S, T>)
        return !at.is_none() && at.value < store.size();
    else if constexpr (sizeof...(Ts) == 1 && ContainsKeysOf<S, T>)
        return !at.is_none() && store.contains(at);
    else if constexpr (SelectsStoreOf<S, T>)
        return urd::contains_key(store.template get<T>(), at);
    else if constexpr (sizeof...(Ts) > 1 && SelectsBy<S, detail::first_part<key<Ts...>>>)
    {
        const auto rest = detail::rest_of(at);
        return store.visit(urd::get<detail::first_part<key<Ts...>>>(at), [rest](const auto& inner) { return urd::contains_key(inner, rest); }, false);
    }
    else
        static_assert(false, "urd::contains_key: a store that holds the key's records, selects their store or selects by the key's first part");
}

/// Whether S enumerates the containers of its own layer that indices of P select: `store.for_each_own<P>(f)` calls f
/// with each (a layered map of containers: only the ones this layer made, not its parent's).
template<typename S, typename P>
concept EnumeratesOwnBy = requires(const S& store) { store.template for_each_own<P>([](const auto& /*inner*/) {}); };

/// Whether a counter of count_at sums over the containers of one layer where no prefix selects one (Count::per_layer: a
/// count of one layer's own records or memory, which the containers of the layer add up to).
template<typename Count>
concept CountsPerLayer = Count::per_layer;


/// What `count` gives for the records of keys K (of kind K::element_type) under the parts `prefix` (its first parts),
/// by the same steps as record_at: a family first selects the store of the kind (store.get<T>()); each part of the
/// prefix selects the container of the next (store.visit: 0 where no layer has it); the store reached that holds the
/// records is counted, count.records(store). With the prefix used up before the last part, the store reached selects
/// containers: a counter of one layer (CountsPerLayer) sums over the containers of its own layer (EnumeratesOwnBy); a
/// count along the layers needs the prefix to the last part (a compile error).
template<typename K, typename S, typename... Ps, typename Count>
[[nodiscard]] std::size_t count_at(const S& store, key<Ps...> prefix, const Count& count)
{
    using T = typename K::element_type;
    if constexpr (SelectsStoreOf<S, T>)
        return urd::count_at<K>(store.template get<T>(), prefix, count);
    else if constexpr (sizeof...(Ps) > 0)
    {
        using P = detail::first_part<key<Ps...>>;
        return store.visit(
            urd::get<P>(prefix),
            [&prefix, &count](const auto& inner) { return urd::count_at<detail::rest_t<K>>(inner, detail::rest_of(prefix), count); },
            std::size_t { 0 });
    }
    else if constexpr (std::same_as<K, key<T>> && HoldsRecordsOf<S, T>)
        return count.records(store);
    else if constexpr (CountsPerLayer<Count> && EnumeratesOwnBy<S, detail::first_part<K>>)
    {
        auto result = std::size_t { 0 };
        store.template for_each_own<detail::first_part<K>>(
            [&result, &count](const auto& inner) { result += urd::count_at<detail::rest_t<K>>(inner, key<>(), count); });
        return result;
    }
    else
        static_assert(false, "urd::count_at: a prefix of the key to the store that holds the records, or a count of one layer over its own containers");
}

/// Whether S inserts records of T, each once: store.insert(content, make) is the index of the record of that content,
/// and whether it was stored now (a pool: deduplicated, along its layers), `make(index)` giving the record to store
/// under a new index (the content, its key's last part written into it), where the store keeps records whole.
template<typename S, typename T>
concept InsertsRecordsOf = requires(S& store, const data<T>& content, const data<T>& (*make)(key<T>) ) {
    { store.insert(content, make) } -> std::same_as<std::pair<key<T>, bool>>;
};

/// Whether S finds records of T: store.find(content) is the index of the record of that content, or none.
template<typename S, typename T>
concept FindsRecordsOf = requires(const S& store, const data<T>& content) {
    { store.find(content) } -> std::same_as<key<T>>;
};

/// Whether S selects, for an insertion, the container an index of P selects, made where there is none (in place over
/// the parent layer's: urd::layered): store.own(key<P>), the container or a handle of it.
template<typename S, typename P>
concept SelectsForInsertBy = requires(S& store, key<P> at) { store.own(at); };

/// Whether S is a family of stores that selects, for an insertion, the store of T's records: store.template get<T>()
/// of a writable family.
template<typename S, typename T>
concept SelectsStoreForInsertOf = requires(S& store) { store.template get<T>(); };

/// The key of a record inserted under the parts of its key it is known by (`known`: all but the last; key<> for a key
/// of one part), and whether it was stored now: the same fold as record_at, one step shorter. A family first selects the
/// store of the record's kind (store.get<T>()); each known part selects the container of the next (store.own: made
/// where there is none); the store reached inserts the content (deduplicated) and gives the last part, which is written
/// into the content (its `index`: a record holds its whole key).
template<typename S, typename... Ps, typename T>
std::pair<key<Ps..., T>, bool> insert_at(S& store, key<Ps...> known, data<T>& content)
{
    if constexpr (sizeof...(Ps) == 0 && InsertsRecordsOf<S, T>)
    {
        const auto [at, inserted] = store.insert(content,
                                                 [&content](key<T> given) -> const data<T>&
                                                 {
                                                     content.index = given;
                                                     return content;
                                                 });
        content.index = at;
        return { at, inserted };
    }
    else if constexpr (SelectsStoreForInsertOf<S, T>)
    {
        auto&& inner = store.template get<T>();
        return urd::insert_at(inner, known, content);
    }
    else if constexpr (sizeof...(Ps) > 0 && SelectsForInsertBy<S, detail::first_part<key<Ps...>>>)
    {
        using P = detail::first_part<key<Ps...>>;
        auto&& inner = store.own(urd::get<P>(known));
        const auto [rest, inserted] = urd::insert_at(inner, detail::rest_of(known), content);
        return { detail::joined_key(key<P>(urd::get<P>(known)), rest), inserted };
    }
    else
        static_assert(false, "urd::insert_at: a store that inserts the records, selects their store or selects by the first known part");
}

/// The key of the record of `content` under the parts `known`, or none where no layer holds it: insert_at's steps,
/// inserting nothing (a known part selects through store.visit).
template<typename S, typename... Ps, typename T>
[[nodiscard]] key<Ps..., T> find_at(const S& store, key<Ps...> known, const data<T>& content)
{
    if constexpr (sizeof...(Ps) == 0 && FindsRecordsOf<S, T>)
        return store.find(content);
    else if constexpr (SelectsStoreOf<S, T>)
        return urd::find_at(store.template get<T>(), known, content);
    else if constexpr (sizeof...(Ps) > 0 && SelectsBy<S, detail::first_part<key<Ps...>>>)
    {
        using P = detail::first_part<key<Ps...>>;
        return store.visit(
            urd::get<P>(known),
            [known, &content](const auto& inner)
            {
                const auto rest = urd::find_at(inner, detail::rest_of(known), content);
                return rest.is_none() ? key<Ps..., T>() : detail::joined_key(key<P>(urd::get<P>(known)), rest);
            },
            key<Ps..., T>());
    }
    else
        static_assert(false, "urd::find_at: a store that finds the records, selects their store or selects by the first known part");
}

}  // namespace urd
