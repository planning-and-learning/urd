#pragma once

// What tests reach of a container beyond what its Store offers: how many nodes a tree pool keeps, a layer's parent, a
// segmented vector's append by reserved index. A
// friend of those containers (tests/exposure.py, rule F7); nothing outside tests uses it.

#include <cstddef>
#include <utility>

namespace urd::testing
{

struct inspect
{
    template<typename R>
    [[nodiscard]] static auto num_nodes(const R& held)
    { return held.num_nodes(); }
    template<typename R>
    [[nodiscard]] static auto num_leaves(const R& held)
    { return held.num_leaves(); }
    template<typename R>
    [[nodiscard]] static auto parent(const R& held)
    { return held.parent(); }
    /// A segmented vector's append that hands the factory the index reserved (what slots number their elements by).
    template<typename R, typename Factory>
    static auto emplace_back_with_index(R& held, Factory&& factory, std::size_t max_index)
    { return held.emplace_back_with_index(std::forward<Factory>(factory), max_index); }
};

}  // namespace urd::testing
