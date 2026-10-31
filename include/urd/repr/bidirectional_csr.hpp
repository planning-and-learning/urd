#pragma once

// A frozen multimap by dense key that answers by value too: compressed sparse rows both ways.

#include "urd/elements.hpp"
#include "urd/repr/grouped_fwd.hpp"
#include "urd/repr/grouped_multimap.hpp"
#include "urd/repr/index_groups.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A CsrMultiMap that also keeps the inverse, values to keys, as compressed sparse rows: a frozen graph with its
/// in-edges. Its values must be indices too.
template<typename K, typename V>
class BidirectionalCsr : public CsrMultiMap<K, V>
{
public:
    BidirectionalCsr() = default;

    template<std::input_iterator InputIt>
    BidirectionalCsr(InputIt first, InputIt last) : BidirectionalCsr(std::vector<std::pair<K, V>>(first, last))
    {
    }

    /// The keys that map to `value`, in the order their pairs were given.
    [[nodiscard]] std::span<const K> keys(const V& value) const { return m_inverse.values(value); }

private:
    explicit BidirectionalCsr(const std::vector<std::pair<K, V>>& pairs) : CsrMultiMap<K, V>(pairs.begin(), pairs.end()), m_inverse(inverted(pairs)) {}
    [[nodiscard]] static CsrMultiMap<V, K> inverted(const std::vector<std::pair<K, V>>& pairs)
    {
        auto swapped = std::vector<std::pair<V, K>>();
        swapped.reserve(pairs.size());
        for (const auto& [key, value] : pairs)
            swapped.emplace_back(value, key);
        return CsrMultiMap<V, K>(swapped.begin(), swapped.end());
    }

    CsrMultiMap<V, K> m_inverse;
};

}  // namespace urd::repr
