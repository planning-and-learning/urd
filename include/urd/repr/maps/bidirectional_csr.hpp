#pragma once

// A frozen multimap by dense key that answers by value too: compressed sparse rows both ways.

#include "urd/elements/requirements.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/maps/grouped_fwd.hpp"
#include "urd/repr/maps/grouped_multimap.hpp"
#include "urd/repr/maps/index_groups.hpp"
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

    friend struct urd::memory_of<BidirectionalCsr>;

    CsrMultiMap<V, K> m_inverse;
};

}  // namespace urd::repr

/// The rows by key, and the inverse rows by value.
template<typename K, typename V>
struct urd::memory_of<urd::repr::BidirectionalCsr<K, V>>
{
    static std::size_t bytes(const repr::BidirectionalCsr<K, V>& held)
    { return memory_usage(static_cast<const repr::CsrMultiMap<K, V>&>(held)) + memory_usage(held.m_inverse); }
};

template<>
struct urd::include<"urd/repr/maps/bidirectional_csr.hpp">
{
};

namespace urd::detail
{

template<typename S>
struct representation_of<bidirectional_csr_decl, S>
{
    using type = repr::BidirectionalCsr<typename S::key_type, typename S::mapped_type>;
};

}  // namespace urd::detail
