#pragma once
// A record held by its handle: a slot of a Store of slots (Store::make_handle), given back when the handle goes. The
// handle names no Store, so that a record may hold handles of its own kind and a library may keep its pools out of its
// headers.
#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace urd
{

/// What gives a record back to the Store of slots it came from when its handle goes: the Store, the record's slot
/// there, and the Store's function that gives it back (its releaser: urd::slot_release, then erase); and the Store's
/// function that draws another slot of it (its record as the slot kept it), so that a handle is cloned into the same
/// Store (urd::clone), where it has one (a Store of keyed slots has none: a slot is drawn under a key).
struct release
{
    void* store = nullptr;
    std::uint64_t slot = 0;
    void (*give_back)(void* store, std::uint64_t slot, void* record) noexcept = nullptr;
    void* (*acquire)(void* store, std::uint64_t& slot) = nullptr;

    void operator()(void* record) const noexcept { give_back(store, slot, record); }
};

/// A record R held by its handle, from a Store of slots of R (Store::make_handle): given back when the handle goes.
template<typename R>
using handle = std::unique_ptr<R, release>;

/// A record held by its handle on the heap, owned by no Store: deleted when its handle goes, and cloned onto the heap
/// (urd::clone). What a child holds where no pool is at hand (a builder made in Python, a test).
template<typename R, typename... Args>
handle<R> make_owned(Args&&... args)
{
    auto release = urd::release {};
    release.give_back = [](void* /*store*/, std::uint64_t /*slot*/, void* record) noexcept { delete static_cast<R*>(record); };
    release.acquire = [](void* /*store*/, std::uint64_t& /*slot*/) -> void* { return new R(); };
    return handle<R>(new R(std::forward<Args>(args)...), release);
}

/// Whether a handle's record is on the heap (make_owned), not in a slot of a Store.
template<typename R>
[[nodiscard]] bool is_owned(const handle<R>& held) noexcept
{ return held.get_deleter().store == nullptr; }

/// A copy of the record a handle holds, in a slot of its own of the same Store (urd/elements/clone.hpp).
template<typename R>
handle<R> clone(const handle<R>& held);

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
