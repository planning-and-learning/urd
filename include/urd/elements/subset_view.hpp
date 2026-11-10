#pragma once

#include "urd/elements/view.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <ranges>
#include <span>

namespace urd
{

/// Some rows of a pool: the pool and the sorted handles of the rows, both borrowed (they must outlive the view, and
/// the pool keep the rows where they are). Its rows by position are the pool's (view(h)); contains finds a row in the
/// pool, then its handle among these. A borrowed range.
template<typename Pool>
class subset_view
{
public:
    using handle_type = typename Pool::handle_type;

    subset_view(const Pool& pool, std::span<const handle_type> handles) noexcept : m_pool(&pool), m_handles(handles)
    { assert(std::ranges::is_sorted(handles)); }

    [[nodiscard]] std::size_t size() const noexcept { return m_handles.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_handles.empty(); }
    /// The i-th row, as the pool hands it out.
    [[nodiscard]] auto operator[](std::size_t i) const
    {
        assert(i < size());
        return m_pool->at(m_handles[i]);
    }
    [[nodiscard]] auto begin() const { return rows().begin(); }
    [[nodiscard]] auto end() const { return rows().end(); }
    /// Whether a row equal to `key` (anything the pool finds by) is among these.
    template<typename Key>
    [[nodiscard]] bool contains(const Key& key) const
    {
        const auto found = m_pool->find(key);
        return !found.is_none() && std::ranges::binary_search(m_handles, found);
    }

private:
    [[nodiscard]] auto rows() const
    { return urd::transform(m_handles, [pool = m_pool](handle_type handle) { return pool->at(handle); }); }

    const Pool* m_pool;
    std::span<const handle_type> m_handles;
};

}  // namespace urd

template<typename Pool>
inline constexpr bool std::ranges::enable_borrowed_range<urd::subset_view<Pool>> = true;
