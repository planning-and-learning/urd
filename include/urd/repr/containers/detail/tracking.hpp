#pragma once

// Which positions of a container were written since its last reset, so that a reset takes time in proportion to them:
// what the dense set (its words) and the jagged rows (their rows) track on request.

#include "urd/memory.hpp"
#include "urd/repr/values/sparse_set.hpp"

#include <cstddef>
#include <type_traits>

namespace urd::repr::detail
{

/// Nothing is tracked: a reset goes over every position.
struct Untracked
{
    static void insert(std::size_t /*position*/) noexcept {}
    static void erase(std::size_t /*position*/) noexcept {}
    static void clear() noexcept {}
    /// Calls reset(first, last) for the positions to reset among the `count` there are: all of them, at once.
    template<typename Reset>
    static void reset(std::size_t count, Reset reset)
    { reset(std::size_t { 0 }, count); }
};

/// The positions written are kept in a sparse set, and a reset visits only those.
struct Touched
{
    void insert(std::size_t position) { positions.insert(position); }
    void erase(std::size_t position) { positions.erase(position); }
    void clear() { positions.clear(); }
    /// Calls reset(position, position + 1) for each position written, and forgets them.
    template<typename Reset>
    void reset(std::size_t /*count*/, Reset reset)
    {
        for (const auto position : positions)
            reset(position, position + 1);
        positions.clear();
    }

    SparseSet<std::size_t> positions;
};

template<bool Track>
using Tracking = std::conditional_t<Track, Touched, Untracked>;

}  // namespace urd::repr::detail

template<>
struct urd::memory_of<urd::repr::detail::Untracked>
{
    static std::size_t bytes(const urd::repr::detail::Untracked& /*held*/) noexcept { return 0; }
};
template<>
struct urd::memory_of<urd::repr::detail::Touched>
{
    static std::size_t bytes(const urd::repr::detail::Touched& held) { return urd::memory_usage(held.positions); }
};
