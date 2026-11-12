#pragma once

// A window of keys: what a dense set is built from where its keys lie in a window (urd/kinds.hpp, builds_from).

namespace urd
{

/// Keys from `first` to before `last`. A dense set built from it keeps its words from `first` on, so that a set of keys
/// in a window costs the window's words, not those below it: no key below `first` fits. `last` is room, not a bound: a
/// key past it grows the set.
template<typename K>
struct key_range
{
    K first;
    K last;
};

}  // namespace urd
