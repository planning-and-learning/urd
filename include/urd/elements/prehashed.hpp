#pragma once

#include "urd/elements/concepts.hpp"
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"

#include <type_traits>

// A key with its hash computed once, for urd's hashed Stores to find and insert by: the standard's way to pass a
// precomputed hash. C++20 had dedicated overloads for it (P0920) and dropped them again (P1661) for this: a key that
// carries its hash, which a transparent hasher returns as it is. So there are no *_with_hash members: find, contains,
// count and an interner's insert take a prehashed<T> wherever they take a T.

namespace urd
{

/// `key` and its Hash<T>: the hash a hashed Store uses for it, without hashing `key` again. It refers to `key`,
/// which must outlive it; the hash must be Hash<T>'s of `key` (prehash gives it).
template<typename T>
struct prehashed
{
    const T& key;
    hash_t hash;
};

/// `key` with its hash, to look it up (or insert it) in several Stores, or more than once, hashing it once.
template<typename T>
[[nodiscard]] prehashed<T> prehash(const T& key) noexcept(noexcept(Hash<T> {}(key)))
{ return { key, Hash<T> {}(key) }; }
/// Not of a temporary: the prehashed key would refer to it after it is gone (as std::ref refuses one).
template<typename T>
void prehash(const T&&) = delete;

/// Whether K is a prehashed<…>.
template<typename K>
inline constexpr bool is_prehashed = false;
template<typename T>
inline constexpr bool is_prehashed<prehashed<T>> = true;

namespace detail
{

/// Hash<T> that also takes a prehashed<T>, giving its hash: what urd's hash containers bind, so that each finds by a
/// key that carries its hash. Transparent, as the standard's heterogeneous lookup needs.
template<typename T>
struct prehash_hash : Hash<T>
{
    using is_transparent = void;
    using Hash<T>::operator();

    [[nodiscard]] constexpr hash_t operator()(const prehashed<T>& key) const noexcept { return key.hash; }
};

/// EqualTo<T> that also compares a prehashed<T> by its key: prehash_hash's partner.
template<typename T>
struct prehash_equal : EqualTo<T>
{
    using is_transparent = void;
    using EqualTo<T>::operator();

    [[nodiscard]] constexpr bool operator()(const prehashed<T>& lhs, const T& rhs) const { return EqualTo<T>::operator()(lhs.key, rhs); }
    [[nodiscard]] constexpr bool operator()(const T& lhs, const prehashed<T>& rhs) const { return EqualTo<T>::operator()(lhs, rhs.key); }
};

}  // namespace detail

}  // namespace urd
