#pragma once

// How a lookup reads, and an insert writes, a container that threads may share: tags a call passes (urd::frozen_read,
// urd::exclusive_write), its default where it passes none.

namespace urd
{

/// A lookup that takes the container's read locks, where threads share it: the default.
struct SharedRead
{
};

/// A lookup of a container no thread changes while it is read (the parent layers of a layered pool, which must not change
/// while layered over): without its read locks.
struct FrozenRead
{
};

/// An insert that takes the container's locks, where threads share it: the default.
struct SharedWrite
{
};

/// An insert by the one thread that uses the container meanwhile (no other thread reads or writes it, as a layer only one
/// worker uses): without its locks, its own lookups too.
struct ExclusiveWrite
{
};

inline constexpr FrozenRead frozen_read {};
inline constexpr ExclusiveWrite exclusive_write {};

}  // namespace urd
