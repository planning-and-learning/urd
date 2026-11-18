#pragma once

// A sorted set in blocks: a sequence of sorted blocks of at most 2 * Block elements each, every element of a block
// before every element of the next, and a Fenwick tree over the blocks' sizes. Insert, erase and lookup are a binary
// search over the blocks' last elements and one within a block, plus a move of at most a block's elements: O(log n);
// an insert after every element (input in order) appends to the last block without a search, O(1) amortized;
// with a block's move as its constant (Python's sortedcontainers.SortedList is built so). The elements have positions:
// an iterator is random-access, reaching a position in O(log n) through the tree. Blocks emptied or left by clear()
// are kept for the next elements (retains).

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/less.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/values/sorted_block_set_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <compare>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

template<>
struct urd::include<"urd/repr/values/sorted_block_set.hpp">
{
};

namespace urd::repr
{

/// A sorted set of T in blocks (see above), ordered by Compare; unique: an element equivalent to one it holds is not
/// inserted. Its iterators are const and random-access.
template<typename T, typename Compare = urd::Less<T>, std::size_t Block = 512>
class SortedBlockSet
{
    static_assert(Block > 0, "urd::SortedBlockSet: a block holds at least one element");

public:
    using value_type = T;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const T&;
    using const_reference = const T&;

    class const_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() = default;

        reference operator*() const { return (*m_set->m_blocks[m_block])[m_offset]; }
        pointer operator->() const { return &**this; }
        reference operator[](difference_type n) const { return *(*this + n); }

        const_iterator& operator++()
        {
            if (++m_offset == m_set->m_blocks[m_block]->size())
            {
                ++m_block;
                m_offset = 0;
            }
            ++m_position;
            return *this;
        }

        const_iterator operator++(int)
        {
            auto before = *this;
            ++*this;
            return before;
        }

        const_iterator& operator--()
        {
            if (m_offset == 0)
                m_offset = m_set->m_blocks[--m_block]->size();
            --m_offset;
            --m_position;
            return *this;
        }

        const_iterator operator--(int)
        {
            auto before = *this;
            --*this;
            return before;
        }

        const_iterator& operator+=(difference_type n)
        {
            *this = m_set->at_position(static_cast<size_type>(static_cast<difference_type>(m_position) + n));
            return *this;
        }

        const_iterator& operator-=(difference_type n) { return *this += -n; }

        friend const_iterator operator+(const_iterator it, difference_type n) { return it += n; }
        friend const_iterator operator+(difference_type n, const_iterator it) { return it += n; }
        friend const_iterator operator-(const_iterator it, difference_type n) { return it -= n; }

        friend difference_type operator-(const const_iterator& lhs, const const_iterator& rhs)
        { return static_cast<difference_type>(lhs.m_position) - static_cast<difference_type>(rhs.m_position); }

        friend bool operator==(const const_iterator& lhs, const const_iterator& rhs) { return lhs.m_position == rhs.m_position; }
        friend std::strong_ordering operator<=>(const const_iterator& lhs, const const_iterator& rhs) { return lhs.m_position <=> rhs.m_position; }

    private:
        friend class SortedBlockSet;

        const_iterator(const SortedBlockSet* set, size_type block, size_type offset, size_type position) :
            m_set(set),
            m_block(block),
            m_offset(offset),
            m_position(position)
        {
        }

        const SortedBlockSet* m_set = nullptr;
        size_type m_block = 0;
        size_type m_offset = 0;
        size_type m_position = 0;
    };

    using iterator = const_iterator;

    SortedBlockSet() = default;
    explicit SortedBlockSet(const Compare& compare) : m_compare(compare) {}

    template<std::input_iterator I>
    SortedBlockSet(I first, I last, const Compare& compare = Compare()) : m_compare(compare)
    { insert(first, last); }

    SortedBlockSet(std::initializer_list<T> elements, const Compare& compare = Compare())
        requires std::copy_constructible<T>
        : SortedBlockSet(elements.begin(), elements.end(), compare)
    {
    }

    SortedBlockSet(const SortedBlockSet& other)
        requires std::copy_constructible<T>
        : m_compare(other.m_compare)
    {
        for (const auto& element : other)
            insert(element);
    }

    SortedBlockSet& operator=(const SortedBlockSet& other)
        requires std::copy_constructible<T>
    {
        if (this != &other)
        {
            clear();
            m_compare = other.m_compare;
            for (const auto& element : other)
                insert(element);
        }
        return *this;
    }

    SortedBlockSet(SortedBlockSet&&) noexcept = default;
    SortedBlockSet& operator=(SortedBlockSet&&) noexcept = default;
    ~SortedBlockSet() = default;

    [[nodiscard]] const_iterator begin() const noexcept { return { this, 0, 0, 0 }; }
    [[nodiscard]] const_iterator end() const noexcept { return { this, m_blocks.size(), 0, m_size }; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return end(); }

    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

    /// The element at a position (0 ≤ position < size()).
    [[nodiscard]] const_reference nth_element(size_type position) const { return *at_position(position); }

    /// The iterator at a position (size(): end()).
    [[nodiscard]] const_iterator nth(size_type position) const { return at_position(position); }

    /// The position of an iterator.
    [[nodiscard]] size_type index_of(const_iterator it) const noexcept { return it.m_position; }

    std::pair<const_iterator, bool> insert(const T& element)
        requires std::copy_constructible<T>
    { return emplace_at(element, [&] { return T(element); }); }

    std::pair<const_iterator, bool> insert(T&& element) { return emplace_at(element, [&] { return T(std::move(element)); }); }

    /// The hint is not used: its place is found by value.
    const_iterator insert(const_iterator /*hint*/, const T& element)
        requires std::copy_constructible<T>
    { return insert(element).first; }

    const_iterator insert(const_iterator /*hint*/, T&& element) { return insert(std::move(element)).first; }

    template<std::input_iterator I>
    void insert(I first, I last)
    {
        for (; first != last; ++first)
            insert(*first);
    }

    template<typename... Args>
    std::pair<const_iterator, bool> emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }

    /// Erases the element at `pos`; the iterator after it.
    const_iterator erase(const_iterator pos)
    {
        const auto position = pos.m_position;
        auto& block = *m_blocks[pos.m_block];
        block.erase(block.begin() + static_cast<difference_type>(pos.m_offset));
        --m_size;
        if (block.empty())
        {
            m_spare.push_back(std::move(m_blocks[pos.m_block]));
            m_blocks.erase(m_blocks.begin() + static_cast<difference_type>(pos.m_block));
            rebuild_tree();
        }
        else
            add_to_tree(pos.m_block, -1);
        return at_position(position);
    }

    const_iterator erase(const_iterator first, const_iterator last)
    {
        const auto position = first.m_position;
        for (auto count = last - first; count > 0; --count)
            erase(at_position(position));
        return at_position(position);
    }

    size_type erase(const T& element)
    {
        const auto it = find(element);
        if (it == end())
            return 0;
        erase(it);
        return 1;
    }

    /// Forgets its elements; their blocks stay, emptied, for the next.
    void clear() noexcept
    {
        for (auto& block : m_blocks)
        {
            block->clear();
            m_spare.push_back(std::move(block));
        }
        m_blocks.clear();
        m_tree.clear();
        m_size = 0;
    }

    /// Lookups take any key its order compares with an element (a transparent order), T included.
    template<typename K = T>
    [[nodiscard]] const_iterator lower_bound(const K& element) const
    {
        const auto block = block_of(element);
        if (block == m_blocks.size())
            return end();
        const auto& held = *m_blocks[block];
        const auto offset = static_cast<size_type>(std::lower_bound(held.begin(), held.end(), element, m_compare) - held.begin());
        return located(block, offset);
    }

    /// The first element after `element`: past every element equivalent to it (with a transparent order, many may be: the
    /// elements that start with a prefix, say).
    template<typename K = T>
    [[nodiscard]] const_iterator upper_bound(const K& element) const
    {
        const auto found = std::partition_point(m_blocks.begin(), m_blocks.end(), [&](const Block_ptr& block) { return !m_compare(element, block->back()); });
        const auto block = static_cast<size_type>(found - m_blocks.begin());
        if (block == m_blocks.size())
            return end();
        const auto& held = *m_blocks[block];
        const auto offset = static_cast<size_type>(std::upper_bound(held.begin(), held.end(), element, m_compare) - held.begin());
        return located(block, offset);
    }

    template<typename K = T>
    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const K& element) const { return { lower_bound(element), upper_bound(element) }; }

    template<typename K = T>
    [[nodiscard]] const_iterator find(const K& element) const
    {
        const auto it = lower_bound(element);
        return it != end() && !m_compare(element, *it) ? it : end();
    }

    template<typename K = T>
    [[nodiscard]] bool contains(const K& element) const { return find(element) != end(); }

    template<typename K = T>
    [[nodiscard]] size_type count(const K& element) const { return contains(element) ? 1 : 0; }

    /// Moves in the elements of `source` it does not hold; those it holds stay in `source`.
    void merge(SortedBlockSet& source)
    {
        for (auto it = source.begin(); it != source.end();)
        {
            if (contains(*it))
                ++it;
            else
            {
                auto& block = *source.m_blocks[it.m_block];
                insert(std::move(block[it.m_offset]));
                it = source.erase(it);
            }
        }
    }

    friend bool operator==(const SortedBlockSet& lhs, const SortedBlockSet& rhs) { return std::ranges::equal(lhs, rhs, urd::EqualTo<> {}); }

    /// What it holds on the heap besides its elements' own: its blocks' capacities, theirs and the tree's.
    [[nodiscard]] std::size_t held_bytes() const noexcept
    {
        auto bytes = m_blocks.capacity() * sizeof(Block_ptr) + m_spare.capacity() * sizeof(Block_ptr) + m_tree.capacity() * sizeof(size_type);
        for (const auto& block : m_blocks)
            bytes += sizeof(std::vector<T>) + block->capacity() * sizeof(T);
        for (const auto& block : m_spare)
            bytes += sizeof(std::vector<T>) + block->capacity() * sizeof(T);
        return bytes;
    }

private:
    // A block behind a pointer, so that moving the sequence of blocks moves no elements.
    using Block_ptr = std::unique_ptr<std::vector<T>>;

    /// Inserts the element `make` constructs where `element` belongs, unless an equivalent one is held.
    template<typename Make>
    std::pair<const_iterator, bool> emplace_at(const T& element, Make make)
    {
        if (m_blocks.empty())
        {
            m_blocks.push_back(take_block());
            m_blocks.back()->push_back(make());
            rebuild_tree();
            ++m_size;
            return { located(0, 0), true };
        }
        if (auto& last = *m_blocks.back(); m_compare(last.back(), element))
        {
            // After every element (input that arrives in order): appended to the last block, without a search.
            const auto block = m_blocks.size() - 1;
            const auto position = m_size;
            last.push_back(make());
            ++m_size;
            if (last.size() > 2 * Block)
                split(block);
            else
                add_to_tree(block, 1);
            return { at_position(position), true };
        }
        auto block = block_of(element);
        if (block == m_blocks.size())
            block = m_blocks.size() - 1;  // after every element: at the end of the last block
        auto& held = *m_blocks[block];
        const auto at = std::lower_bound(held.begin(), held.end(), element, m_compare);
        const auto offset = static_cast<size_type>(at - held.begin());
        if (at != held.end() && !m_compare(element, *at))
            return { located(block, offset), false };
        // Its position, taken before the insertion: `make` may move from `element`.
        const auto position = prefix(block) + offset;
        held.insert(at, make());
        ++m_size;
        if (held.size() > 2 * Block)
            split(block);
        else
            add_to_tree(block, 1);
        return { at_position(position), true };
    }

    /// The first block whose last element is not before `element` (that block holds it, or would), or the number of
    /// blocks where every element is before it.
    template<typename K>
    [[nodiscard]] size_type block_of(const K& element) const
    {
        const auto it = std::partition_point(m_blocks.begin(), m_blocks.end(), [&](const Block_ptr& block) { return m_compare(block->back(), element); });
        return static_cast<size_type>(it - m_blocks.begin());
    }

    /// Splits a full block into two halves.
    void split(size_type block)
    {
        auto upper = take_block();
        auto& lower = *m_blocks[block];
        const auto half = lower.begin() + static_cast<difference_type>(lower.size() / 2);
        upper->insert(upper->end(), std::make_move_iterator(half), std::make_move_iterator(lower.end()));
        lower.erase(half, lower.end());
        m_blocks.insert(m_blocks.begin() + static_cast<difference_type>(block) + 1, std::move(upper));
        rebuild_tree();
    }

    /// An empty block: a kept one, else a new one.
    Block_ptr take_block()
    {
        if (m_spare.empty())
        {
            // Room for every block ever made among the spare ones, so that clear() moves them there without allocating.
            m_spare.reserve(m_blocks.size() + 1);
            return std::make_unique<std::vector<T>>();  // it grows as it fills, so a small set holds little
        }
        // The kept block of the largest capacity, so that a set filled again reuses the buffers it had before growing new
        // ones (a block is taken only when one fills up or the set is refilled).
        const auto largest = std::ranges::max_element(m_spare, {}, [](const Block_ptr& kept) { return kept->capacity(); });
        auto block = std::move(*largest);
        *largest = std::move(m_spare.back());
        m_spare.pop_back();
        return block;
    }

    [[nodiscard]] const_iterator located(size_type block, size_type offset) const
    {
        if (offset == m_blocks[block]->size())
            return { this, block + 1, 0, prefix(block + 1) };
        return { this, block, offset, prefix(block) + offset };
    }

    /// The iterator at a position (size(): end()), found by descending the tree.
    [[nodiscard]] const_iterator at_position(size_type position) const
    {
        if (position >= m_size)
            return end();
        // Fenwick descent: the last block whose prefix of sizes is at most `position`.
        size_type block = 0;
        size_type remaining = position;
        auto step = std::bit_floor(std::max<size_type>(m_tree.size(), 1));
        for (; step > 0; step /= 2)
            if (block + step <= m_tree.size() && m_tree[block + step - 1] <= remaining)
            {
                block += step;
                remaining -= m_tree[block - 1];
            }
        return { this, block, remaining, position };
    }

    /// The number of elements in the blocks before `block`.
    [[nodiscard]] size_type prefix(size_type block) const noexcept
    {
        size_type sum = 0;
        for (; block > 0; block -= block & (~block + 1))
            sum += m_tree[block - 1];
        return sum;
    }

    void add_to_tree(size_type block, difference_type delta) noexcept
    {
        for (++block; block <= m_tree.size(); block += block & (~block + 1))
            m_tree[block - 1] = static_cast<size_type>(static_cast<difference_type>(m_tree[block - 1]) + delta);
    }

    /// The tree over the blocks' sizes, built again: after a block is added or removed (every Block insertions, or a
    /// block emptied).
    void rebuild_tree()
    {
        m_tree.resize(m_blocks.size());
        for (size_type i = 0; i < m_blocks.size(); ++i)
            m_tree[i] = m_blocks[i]->size();
        for (size_type i = 1; i <= m_tree.size(); ++i)
            if (const auto parent = i + (i & (~i + 1)); parent <= m_tree.size())
                m_tree[parent - 1] += m_tree[i - 1];
    }

    std::vector<Block_ptr> m_blocks;
    std::vector<Block_ptr> m_spare;
    std::vector<size_type> m_tree;
    size_type m_size = 0;
    [[no_unique_address]] Compare m_compare {};
};

}  // namespace urd::repr

namespace urd::detail
{

template<typename S>
struct representation_of<sorted_block_set_decl, S>
{
    using type = repr::SortedBlockSet<typename S::element_type, typename S::order_type>;
};

}  // namespace urd::detail

namespace urd
{

/// A sorted block set is a value range: hashed, compared and ordered as its elements, in order.
template<typename T, typename Compare, std::size_t Block>
inline constexpr bool value_range<repr::SortedBlockSet<T, Compare, Block>> = true;

/// It owns its blocks, and what its elements hold.
template<typename T, typename Compare, std::size_t Block>
struct memory_of<repr::SortedBlockSet<T, Compare, Block>>
{
    static std::size_t bytes(const repr::SortedBlockSet<T, Compare, Block>& held) { return held.held_bytes() + detail::held_bytes(held); }
};

}  // namespace urd
