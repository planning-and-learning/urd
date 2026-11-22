#pragma once
// The record of a key in nested containers: a key<P..., T> addresses its record level by level, the first part
// selecting a container (a map's value), the next looking up in that, and so on, the last part numbering the record in
// the innermost container (a pool of data<T> handing out index<T>); a family of stores (one per record type) first
// selects the store of the key's kind. One fold for any number of parts and any store: an index, key<T>, is its
// store's own lookup. A context's operator[](key) folds through here.
#include "urd/elements/data.hpp"
#include "urd/elements/index.hpp"

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
concept HandsOutRecordsOf = HandsOutRecordsBy<S, T, index<T>>;

/// Whether S holds records of T by their index (HandsOutRecordsOf), and `store.size()` is the number of indices along
/// its layers (an index below it is held). The innermost container of a nest (a pool of data<T>).
template<typename S, typename T>
concept HoldsRecordsOf = HandsOutRecordsOf<S, T> && requires(const S& store) {
    { store.size() } -> std::convertible_to<std::size_t>;
};

/// Whether S selects a container by an index of P: `store.at(index<P>)` is the container the index selects, from
/// whichever layer holds it (std::out_of_range where none does), and `store.visit(index<P>, f, missing)` is f of it,
/// or `missing` (a layered map of containers: urd::layered).
template<typename S, typename P>
concept SelectsBy = requires(const S& store, index<P> at) {
    store.at(at);
    { store.visit(at, [](const auto& /*inner*/) { return true; }, false) } -> std::same_as<bool>;
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

/// The record of a key, handed out by value as its stored_ref, one step at a time: from a context that hands it out
/// (HandsOutStoredRefs: a repository); an index in a store holding its records, the stored_ref made from the store's
/// (store[at]); a family first selects the store of the key's kind (store.get<T>()); a key of several parts selects a
/// container by its first part (store.at(first)) and folds the rest there. std::out_of_range where a selection finds
/// nothing; an index no layer holds is not looked for (contains_key). The steps are the named concepts above; each is
/// declared before any is defined, so that any step may follow any other. An index is always the store's own step: the
/// most specialized.
template<typename S, typename T>
    requires HandsOutRecordsOf<S, T>
[[nodiscard]] stored_ref<T> record_at(const S& store, key<T> at);

/// The record of T for a handle of any shape its context hands records out by (a view's handle): stored_ref<T> made
/// from context[handle]. T is named, as a handle of the context's own shape does not say it.
template<typename T, typename C, typename H>
    requires HandsOutRecordsBy<C, T, H>
[[nodiscard]] stored_ref<T> record_at(const C& context, const H& handle);

template<typename C, typename... Ts>
    requires(sizeof...(Ts) > 1) && HandsOutStoredRefs<C, key<Ts...>>
[[nodiscard]] stored_ref<typename key<Ts...>::element_type> record_at(const C& context, key<Ts...> at);

template<typename S, typename... Ts>
    requires SelectsStoreOf<S, typename key<Ts...>::element_type>
[[nodiscard]] stored_ref<typename key<Ts...>::element_type> record_at(const S& store, key<Ts...> at);

template<typename S, typename P, typename... Ts>
    requires(sizeof...(Ts) > 0) && SelectsBy<S, P>
[[nodiscard]] stored_ref<typename key<P, Ts...>::element_type> record_at(const S& store, key<P, Ts...> at);

/// Whether some layer holds the key, by the same steps: an index below its store's size; in a family, in the store of
/// its kind; a key of several parts, where its first part selects a container (store.visit) that holds the rest.
template<typename S, typename T>
    requires HoldsRecordsOf<S, T>
[[nodiscard]] bool contains_key(const S& store, key<T> at);

template<typename S, typename... Ts>
    requires SelectsStoreOf<S, typename key<Ts...>::element_type>
[[nodiscard]] bool contains_key(const S& store, key<Ts...> at);

template<typename S, typename P, typename... Ts>
    requires(sizeof...(Ts) > 0) && SelectsBy<S, P>
[[nodiscard]] bool contains_key(const S& store, key<P, Ts...> at);

template<typename S, typename T>
    requires HandsOutRecordsOf<S, T>
stored_ref<T> record_at(const S& store, key<T> at)
{ return stored_ref<T>(store[at]); }

template<typename T, typename C, typename H>
    requires HandsOutRecordsBy<C, T, H>
stored_ref<T> record_at(const C& context, const H& handle)
{ return stored_ref<T>(context[handle]); }

template<typename C, typename... Ts>
    requires(sizeof...(Ts) > 1) && HandsOutStoredRefs<C, key<Ts...>>
stored_ref<typename key<Ts...>::element_type> record_at(const C& context, key<Ts...> at)
{ return context[at]; }

template<typename S, typename... Ts>
    requires SelectsStoreOf<S, typename key<Ts...>::element_type>
stored_ref<typename key<Ts...>::element_type> record_at(const S& store, key<Ts...> at)
{ return urd::record_at(store.template get<typename key<Ts...>::element_type>(), at); }

template<typename S, typename P, typename... Ts>
    requires(sizeof...(Ts) > 0) && SelectsBy<S, P>
stored_ref<typename key<P, Ts...>::element_type> record_at(const S& store, key<P, Ts...> at)
{ return urd::record_at(store.at(urd::get<P>(at)), key<Ts...>(urd::get<Ts>(at)...)); }

template<typename S, typename T>
    requires HoldsRecordsOf<S, T>
bool contains_key(const S& store, key<T> at)
{ return !at.is_none() && at.value < store.size(); }

template<typename S, typename... Ts>
    requires SelectsStoreOf<S, typename key<Ts...>::element_type>
bool contains_key(const S& store, key<Ts...> at)
{ return urd::contains_key(store.template get<typename key<Ts...>::element_type>(), at); }

template<typename S, typename P, typename... Ts>
    requires(sizeof...(Ts) > 0) && SelectsBy<S, P>
bool contains_key(const S& store, key<P, Ts...> at)
{
    const auto rest = key<Ts...>(urd::get<Ts>(at)...);
    return store.visit(urd::get<P>(at), [rest](const auto& inner) { return urd::contains_key(inner, rest); }, false);
}

/// Whether S inserts records of T, each once: store.insert(content, make) is the index of the record of that content,
/// and whether it was stored now (a pool: deduplicated, along its layers), `make(index)` giving the record to store
/// under a new index (the content, its key's last part written into it), where the store keeps records whole.
template<typename S, typename T>
concept InsertsRecordsOf = requires(S& store, const data<T>& content, const data<T>& (*make)(index<T>)) {
    { store.insert(content, make) } -> std::same_as<std::pair<index<T>, bool>>;
};

/// Whether S finds records of T: store.find(content) is the index of the record of that content, or none.
template<typename S, typename T>
concept FindsRecordsOf = requires(const S& store, const data<T>& content) {
    { store.find(content) } -> std::same_as<index<T>>;
};

/// Whether S selects, for an insertion, the container an index of P selects, made where there is none (in place over
/// the parent layer's: urd::layered): store.own(index<P>), the container or a handle of it.
template<typename S, typename P>
concept SelectsForInsertBy = requires(S& store, index<P> at) { store.own(at); };

/// Whether S is a family of stores that selects, for an insertion, the store of T's records: store.template get<T>()
/// of a writable family.
template<typename S, typename T>
concept SelectsStoreForInsertOf = requires(S& store) { store.template get<T>(); };

/// The key of a record inserted under the parts of its key it is known by (`known`: all but the last; key<> for a key
/// of one part), and whether it was stored now: the same fold as record_at, one step shorter. A family first selects the
/// store of the record's kind (store.get<T>()); each known part selects the container of the next (store.own: made
/// where there is none); the store reached inserts the content (deduplicated) and gives the last part, which is written
/// into the content (its `index`: a record holds its whole key).
template<typename S, typename T>
    requires InsertsRecordsOf<S, T>
std::pair<key<T>, bool> insert_at(S& store, key<> known, data<T>& content);

template<typename S, typename... Ts, typename T>
    requires SelectsStoreForInsertOf<S, T>
std::pair<key<Ts..., T>, bool> insert_at(S& store, key<Ts...> known, data<T>& content);

template<typename S, typename P, typename T>
    requires SelectsForInsertBy<S, P>
std::pair<key<P, T>, bool> insert_at(S& store, key<P> known, data<T>& content);

template<typename S, typename P, typename Q, typename... Ps, typename T>
    requires SelectsForInsertBy<S, P>
std::pair<key<P, Q, Ps..., T>, bool> insert_at(S& store, key<P, Q, Ps...> known, data<T>& content);

/// The key of the record of `content` under the parts `known`, or none where no layer holds it: insert_at's steps,
/// inserting nothing (a known part selects through store.visit).
template<typename S, typename T>
    requires FindsRecordsOf<S, T>
[[nodiscard]] key<T> find_at(const S& store, key<> known, const data<T>& content);

template<typename S, typename... Ts, typename T>
    requires SelectsStoreOf<S, T>
[[nodiscard]] key<Ts..., T> find_at(const S& store, key<Ts...> known, const data<T>& content);

template<typename S, typename P, typename T>
    requires SelectsBy<S, P>
[[nodiscard]] key<P, T> find_at(const S& store, key<P> known, const data<T>& content);

template<typename S, typename P, typename Q, typename... Ps, typename T>
    requires SelectsBy<S, P>
[[nodiscard]] key<P, Q, Ps..., T> find_at(const S& store, key<P, Q, Ps...> known, const data<T>& content);

template<typename S, typename T>
    requires InsertsRecordsOf<S, T>
std::pair<key<T>, bool> insert_at(S& store, key<> /*known*/, data<T>& content)
{
    const auto [at, inserted] = store.insert(content,
                                             [&content](index<T> given) -> const data<T>&
                                             {
                                                 content.index = given;
                                                 return content;
                                             });
    content.index = at;
    return { at, inserted };
}

template<typename S, typename... Ts, typename T>
    requires SelectsStoreForInsertOf<S, T>
std::pair<key<Ts..., T>, bool> insert_at(S& store, key<Ts...> known, data<T>& content)
{
    auto&& inner = store.template get<T>();
    return urd::insert_at(inner, known, content);
}

template<typename S, typename P, typename T>
    requires SelectsForInsertBy<S, P>
std::pair<key<P, T>, bool> insert_at(S& store, key<P> known, data<T>& content)
{
    auto&& inner = store.own(known);
    const auto [last, inserted] = urd::insert_at(inner, key<>(), content);
    return { key<P, T>(known, last), inserted };
}

template<typename S, typename P, typename Q, typename... Ps, typename T>
    requires SelectsForInsertBy<S, P>
std::pair<key<P, Q, Ps..., T>, bool> insert_at(S& store, key<P, Q, Ps...> known, data<T>& content)
{
    auto&& inner = store.own(urd::get<P>(known));
    const auto [rest, inserted] = urd::insert_at(inner, key<Q, Ps...>(urd::get<Q>(known), urd::get<Ps>(known)...), content);
    return { key<P, Q, Ps..., T>(urd::get<P>(known), urd::get<Q>(rest), urd::get<Ps>(rest)..., urd::get<T>(rest)), inserted };
}

template<typename S, typename T>
    requires FindsRecordsOf<S, T>
key<T> find_at(const S& store, key<> /*known*/, const data<T>& content)
{ return store.find(content); }

template<typename S, typename... Ts, typename T>
    requires SelectsStoreOf<S, T>
key<Ts..., T> find_at(const S& store, key<Ts...> known, const data<T>& content)
{ return urd::find_at(store.template get<T>(), known, content); }

template<typename S, typename P, typename T>
    requires SelectsBy<S, P>
key<P, T> find_at(const S& store, key<P> known, const data<T>& content)
{
    return store.visit(
        known,
        [known, &content](const auto& inner)
        {
            const auto last = urd::find_at(inner, key<>(), content);
            return last.is_none() ? key<P, T>() : key<P, T>(known, last);
        },
        key<P, T>());
}

template<typename S, typename P, typename Q, typename... Ps, typename T>
    requires SelectsBy<S, P>
key<P, Q, Ps..., T> find_at(const S& store, key<P, Q, Ps...> known, const data<T>& content)
{
    return store.visit(
        urd::get<P>(known),
        [known, &content](const auto& inner)
        {
            const auto rest = urd::find_at(inner, key<Q, Ps...>(urd::get<Q>(known), urd::get<Ps>(known)...), content);
            return rest.is_none() ? key<P, Q, Ps..., T>()
                                  : key<P, Q, Ps..., T>(urd::get<P>(known), urd::get<Q>(rest), urd::get<Ps>(rest)..., urd::get<T>(rest));
        },
        key<P, Q, Ps..., T>());
}

}  // namespace urd
