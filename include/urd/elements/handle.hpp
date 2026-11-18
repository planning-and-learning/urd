#pragma once
// A record held by its handle: a slot of a Store of slots (Store::make_handle), given back when the handle goes. The
// handle names no Store, so that a record may hold handles of its own kind and a library may keep its pools out of its
// headers.
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"

#include <cstdint>
#include <memory>

namespace urd
{

/// What gives a record back to the Store of slots it came from when its handle goes: the Store, the record's slot
/// there, and the Store's function that gives it back (its releaser: urd::slot_release, then erase).
struct release
{
    void* store = nullptr;
    std::uint64_t slot = 0;
    void (*give_back)(void* store, std::uint64_t slot, void* record) noexcept = nullptr;

    void operator()(void* record) const noexcept { give_back(store, slot, record); }
};

/// A record R held by its handle, from a Store of slots of R (Store::make_handle): given back when the handle goes.
template<typename R>
using handle = std::unique_ptr<R, release>;

/// A record held by its handle equals another as the records they hold; no record equals only none.
template<typename R>
struct EqualTo<handle<R>>
{
    // Unconstrained: a record may hold handles of its own kind (a call stack's caller), whose constraint would recurse.
    bool operator()(const handle<R>& lhs, const handle<R>& rhs) const { return lhs == nullptr || rhs == nullptr ? lhs == rhs : EqualTo<R> {}(*lhs, *rhs); }
};

/// A record held by its handle hashes as the record it holds.
template<typename R>
struct Hash<handle<R>>
{
    hash_t operator()(const handle<R>& held) const { return held == nullptr ? hash_t { 0 } : Hash<R> {}(*held); }
};

}  // namespace urd
