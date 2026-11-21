#pragma once
// urd::copy: a record of one context written into another of its kinds (a repository layer into a sibling, say), as
// urd::clone copies a builder: where the target sees the record's context (to.sees(source): a key there names the same
// record), its key as it is; otherwise each child copied first (through the fields, as build_from builds them), the
// values as they are, and the record interned into the target. Included only by the sources that copy.
#include "urd/elements/builder.hpp"
#include "urd/elements/children.hpp"
#include "urd/elements/index.hpp"
#include "urd/elements/view.hpp"

#include <concepts>
#include <stdexcept>

namespace urd
{
/// Whether a context tells whether it sees the records of another context of its type (`to.sees(source)`): whether a
/// key of `source` names the same record in `to` (`source` is `to`, or a layer `to` lies over). Contexts of different
/// types never see each other.
template<typename To>
concept Sees = requires(const To& to, const To& source) {
    { to.sees(source) } -> std::same_as<bool>;
};

/// Whether a context reaches records of kind T through another context of type C, one it nests (`to.repository_for<T>()`:
/// that context): a key of such a record names it there, as it is.
template<typename To, typename T, typename C>
concept Reaches = requires(const To& to) {
    { to.template repository_for<T>() } -> std::same_as<const C&>;
};

/// Whether a context checks that it may hold copies of another's records (`to.check_compatible(source)`, throwing
/// std::invalid_argument where it may not): one whose records name records further down by plain values too (a nesting
/// repository: each repository it nests must see the one `source` nests). A context without such values declares none.
template<typename To>
concept ChecksCompatibility = requires(const To& to, const To& source) { to.check_compatible(source); };

template<typename T>
struct copying;

/// The key in `to` of the record a stored view reads: its own where `to` sees its context, else its copy's (once `to` has
/// checked the context: ChecksCompatibility, where `to` declares the check), its children copied first and its builder drawn from `storage` (checkout<T>()) and
/// interned into `to` (insert(builder, storage)). A record of a kind `to` reaches through a context it nests (Reaches) is not copied: its key where that
/// context sees the record's, else std::invalid_argument.
template<typename T, typename C, typename K, Sees To, typename Storage>
K copy(const index_view<T, C, K>& view, To& to, Storage& storage);

/// The key in `to` of a copy of a record held outside a repository (a builder's, a record not interned yet): never seen,
/// so always copied, once `to` has checked its context (ChecksCompatibility, where `to` declares the check).
template<typename T, typename C, typename R, Sees To, typename Storage>
typename copying<T>::key_type copy(const borrowed_view<T, C, R>& view, To& to, Storage& storage);

/// How a record of kind T is copied into a context that does not see it: through its fields, each child copied first
/// (urd::copy), its values as they are, then interned. A library specializes it for a kind whose record is not read field
/// by field (a row stored packed, say).
template<typename T>
struct copying
{
    using key_type = index<T>;

    template<typename V, typename To, typename Storage>
    static key_type into(const V& view, To& to, Storage& storage)
    {
        auto built = storage.template checkout<T>();
        urd::build_from(view, *built, [&to, &storage](const auto& child) { return urd::copy(child, to, storage); });
        return to.insert(*built, storage).first.index();
    }
};

template<typename T, typename C, typename K, Sees To, typename Storage>
K copy(const index_view<T, C, K>& view, To& to, Storage& storage)
{
    if constexpr (std::same_as<C, To>)
    {
        if (to.sees(view.context()))
            return view.index();
        if constexpr (ChecksCompatibility<To>)
            to.check_compatible(view.context());
        return copying<T>::into(view, to, storage);
    }
    else if constexpr (Reaches<To, T, C>)
    {
        if (to.template repository_for<T>().sees(view.context()))
            return view.index();
        throw std::invalid_argument("urd::copy: a record of a context its target nests but does not see");
    }
    else
        return copying<T>::into(view, to, storage);
}

template<typename T, typename C, typename R, Sees To, typename Storage>
typename copying<T>::key_type copy(const borrowed_view<T, C, R>& view, To& to, Storage& storage)
{
    if constexpr (std::same_as<C, To> && ChecksCompatibility<To>)
        to.check_compatible(view.context());
    return copying<T>::into(view, to, storage);
}

}  // namespace urd
