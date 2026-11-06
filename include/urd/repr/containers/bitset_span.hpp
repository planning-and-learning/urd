#pragma once

// BitsetSpan, a bitset over blocks it does not own (the caller keeps the trailing bits of the last block clear), and
// SetBitIndices, its set bits as a range; their comparison, urd::Hash, and for_each_bit over several spans at once.

#include "urd/elements/hash.hpp"
#include "urd/repr/containers/bit.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace urd::repr
{

/// T and U are the same type but for const.
template<typename T, typename U>
concept SameAsIgnoringConst = std::same_as<std::remove_const_t<T>, std::remove_const_t<U>>;

template<std::unsigned_integral Block>
class BitsetSpan
{
private:
    template<std::unsigned_integral>
    friend class BitsetSpan;

public:
    using U = std::remove_const_t<Block>;

    using bit_reference = bit::bit_reference<U>;

    static constexpr std::size_t Digits = std::numeric_limits<U>::digits;
    static constexpr std::size_t BlockShift = std::countr_zero(Digits);
    static constexpr std::size_t BlockMask = Digits - 1;

    static constexpr std::size_t block_index(std::size_t pos) noexcept { return pos >> BlockShift; }
    static constexpr std::size_t block_pos(std::size_t pos) noexcept { return pos & BlockMask; }
    static constexpr std::size_t num_blocks(std::size_t num_bits) noexcept { return (num_bits + Digits - 1) >> BlockShift; }

    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    static constexpr U full_mask() noexcept { return static_cast<U>(~U { 0 }); }

    static constexpr U last_mask(std::size_t num_bits) noexcept
    {
        const std::size_t r = num_bits % Digits;
        return (r == 0) ? full_mask() : bit::lo_set<U>[r];
    }

public:
    BitsetSpan(Block* data, std::size_t num_bits) noexcept : m_data(data), m_num_bits(num_bits) {}

    template<typename OtherBlock>
        requires(std::is_const_v<Block> && !std::is_const_v<OtherBlock> && std::same_as<std::remove_const_t<OtherBlock>, U>)
    BitsetSpan(const BitsetSpan<OtherBlock>& other) noexcept : m_data(other.m_data), m_num_bits(other.m_num_bits)
    {
    }

    /**
     * Helpers
     */

    void clear_trailing_bits() noexcept
        requires(!std::is_const_v<Block>)
    {
        const std::size_t n = num_blocks(m_num_bits);
        if (n == 0)
            return;
        blocks()[n - 1] &= last_mask(m_num_bits);
    }

    [[nodiscard]] bool trailing_bits_zero() const noexcept
    {
        const std::size_t n = num_blocks(m_num_bits);
        if (n == 0)
            return true;

        const U mask = last_mask(m_num_bits);
        return (blocks()[n - 1] & ~mask) == U { 0 };
    }

    /**
     * Accessors
     */

    [[nodiscard]] bool test(std::size_t pos) const noexcept
    {
        assert(pos < m_num_bits);

        return (blocks()[block_index(pos)] & (U { 1 } << block_pos(pos))) != U { 0 };
    }

    [[nodiscard]] bool at(std::size_t pos) const
    {
        ensure_storage();
        if (pos >= m_num_bits)
            throw std::out_of_range("BitsetSpan: bit index out of range.");

        return test(pos);
    }

    bool operator[](std::size_t pos) const noexcept { return test(pos); }

    bit_reference operator[](std::size_t pos) noexcept
        requires(!std::is_const_v<Block>)
    {
        assert(pos < m_num_bits);

        return bit_reference(blocks(), pos);
    }

    bit_reference at(std::size_t pos)
        requires(!std::is_const_v<Block>)
    {
        ensure_storage();
        if (pos >= m_num_bits)
            throw std::out_of_range("BitsetSpan: bit index out of range.");

        return (*this)[pos];
    }

    [[nodiscard]] std::size_t count() const noexcept
    {
        assert(trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        std::size_t cnt = 0;

        for (std::size_t i = 0; i < n; ++i)
            cnt += static_cast<std::size_t>(std::popcount(blocks()[i]));

        return cnt;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] std::size_t count_intersection(const BitsetSpan<OtherBlock>& other) const
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());
        std::size_t result = 0;
        for (std::size_t i = 0; i < num_blocks(m_num_bits); ++i)
            result += static_cast<std::size_t>(std::popcount(static_cast<U>(blocks()[i] & other.blocks()[i])));
        return result;
    }

    [[nodiscard]] std::size_t count_zeros() const noexcept
    {
        assert(trailing_bits_zero());

        return m_num_bits - count();
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_num_bits; }

    [[nodiscard]] bool empty() const noexcept { return m_num_bits == 0; }

    [[nodiscard]] bool any() const noexcept
    {
        assert(trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            if (blocks()[i] != U { 0 })
                return true;
        return false;
    }

    [[nodiscard]] bool none() const noexcept { return !any(); }

    [[nodiscard]] bool all() const noexcept
    {
        assert(trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        if (n == 0)
            return true;

        for (std::size_t i = 0; i + 1 < n; ++i)
            if (blocks()[i] != full_mask())
                return false;

        return blocks()[n - 1] == last_mask(m_num_bits);
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] bool intersects(const BitsetSpan<OtherBlock>& other) const
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            if ((blocks()[i] & other.blocks()[i]) != U { 0 })
                return true;

        return false;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] bool is_subset_of(const BitsetSpan<OtherBlock>& other) const
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            if ((blocks()[i] & ~other.blocks()[i]) != U { 0 })
                return false;

        return true;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] bool is_proper_subset_of(const BitsetSpan<OtherBlock>& other) const
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        bool proper = false;
        for (std::size_t i = 0; i < n; ++i)
        {
            if ((blocks()[i] & ~other.blocks()[i]) != U { 0 })
                return false;
            proper = proper || (blocks()[i] != other.blocks()[i]);
        }

        return proper;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] bool is_superset_of(const BitsetSpan<OtherBlock>& other) const
    { return other.is_subset_of(*this); }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    [[nodiscard]] bool is_proper_superset_of(const BitsetSpan<OtherBlock>& other) const
    { return other.is_proper_subset_of(*this); }

    /**
     * Modifiers
     */

    void set(std::size_t pos) noexcept
        requires(!std::is_const_v<Block>)
    {
        assert(pos < m_num_bits);

        blocks()[block_index(pos)] |= static_cast<U>(U { 1 } << block_pos(pos));
    }

    void set(std::size_t pos, bool value) noexcept
        requires(!std::is_const_v<Block>)
    {
        if (value)
            set(pos);
        else
            reset(pos);
    }

    void set() noexcept
        requires(!std::is_const_v<Block>)
    {
        std::ranges::fill(blocks(), full_mask());

        clear_trailing_bits();
    }

    void reset(std::size_t pos) noexcept
        requires(!std::is_const_v<Block>)
    {
        assert(pos < m_num_bits);

        blocks()[block_index(pos)] &= (~(U { 1 } << block_pos(pos)));
    }

    void reset() noexcept
        requires(!std::is_const_v<Block>)
    { std::ranges::fill(blocks(), U { 0 }); }

    void flip(std::size_t pos) noexcept
        requires(!std::is_const_v<Block>)
    {
        assert(pos < m_num_bits);

        blocks()[block_index(pos)] ^= (U { 1 } << block_pos(pos));
    }

    void flip() noexcept
        requires(!std::is_const_v<Block>)
    {
        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] = ~blocks()[i];

        clear_trailing_bits();
    }

    /**
     * Iterators
     */

    [[nodiscard]] std::size_t find_first() const noexcept
    {
        assert(trailing_bits_zero());
        return find_from(0, U { 0 });
    }

    [[nodiscard]] std::size_t find_next(std::size_t pos) const noexcept
    {
        assert(trailing_bits_zero());
        return pos >= m_num_bits ? npos : find_from(pos + 1, U { 0 });
    }

    [[nodiscard]] std::size_t find_first_zero() const noexcept { return find_from(0, full_mask()); }

    [[nodiscard]] std::size_t find_next_zero(std::size_t pos) const noexcept { return pos >= m_num_bits ? npos : find_from(pos + 1, full_mask()); }

    /**
     * Operators
     */

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& copy_from(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] = other.blocks()[i];

        return *this;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& diff_from(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] = other.blocks()[i] & ~blocks()[i];

        return *this;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& operator&=(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] &= other.blocks()[i];

        return *this;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& operator|=(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] |= other.blocks()[i];

        return *this;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& operator^=(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] ^= other.blocks()[i];

        return *this;
    }

    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    BitsetSpan& operator-=(const BitsetSpan<OtherBlock>& other)
        requires(!std::is_const_v<Block>)
    {
        check_same_size(other);
        assert(trailing_bits_zero());
        assert(other.trailing_bits_zero());

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
            blocks()[i] &= ~other.blocks()[i];

        return *this;
    }

    /**
     * Getters
     */

    std::span<U> blocks() noexcept
        requires(!std::is_const_v<Block>)
    { return { m_data, num_blocks(m_num_bits) }; }

    [[nodiscard]] std::span<const U> blocks() const noexcept { return { m_data, num_blocks(m_num_bits) }; }
    U* data() noexcept
        requires(!std::is_const_v<Block>)
    { return m_data; }
    [[nodiscard]] const U* data() const noexcept { return m_data; }
    [[nodiscard]] std::size_t num_bits() const noexcept { return m_num_bits; }

private:
    template<std::unsigned_integral OtherBlock>
        requires SameAsIgnoringConst<OtherBlock, U>
    void check_same_size(const BitsetSpan<OtherBlock>& other) const
    {
        if (m_num_bits != other.m_num_bits)
            throw std::invalid_argument("BitsetSpan operands must have the same size.");
    }

    void ensure_storage() const
    {
        if (m_num_bits > 0 && m_data == nullptr)
            throw std::logic_error("BitsetSpan: invalid span.");
    }

    /// The first position at or after `pos` whose bit differs from the same bit of `flip`, else npos.
    [[nodiscard]] std::size_t find_from(std::size_t pos, U flip) const noexcept
    {
        if (pos >= m_num_bits)
            return npos;
        const std::size_t n = num_blocks(m_num_bits);
        std::size_t i = block_index(pos);
        U w = (blocks()[i] ^ flip) & (full_mask() << block_pos(pos));
        for (;;)
        {
            if (i + 1 == n)
                w &= last_mask(m_num_bits);
            if (w != U { 0 })
                return (i * Digits) + static_cast<std::size_t>(std::countr_zero(w));
            if (++i == n)
                return npos;
            w = blocks()[i] ^ flip;
        }
    }

    Block* m_data;
    std::size_t m_num_bits;
};

/// Iterators borrow the bit storage, independently of the span and range wrappers.
/// Keep the storage alive and unchanged throughout iteration.
template<std::unsigned_integral Block>
class SetBitIndices : public std::ranges::view_interface<SetBitIndices<Block>>
{
    using Bitset = BitsetSpan<const Block>;
    Bitset m_bits { nullptr, 0 };

public:
    class Iterator
    {
        Bitset m_bits { nullptr, 0 };
        std::size_t m_word_offset = Bitset::npos;
        typename Bitset::U m_remaining = 0;

    public:
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;

        Iterator() noexcept = default;
        Iterator(Bitset bits, std::size_t position) noexcept : m_bits(bits), m_word_offset(position == Bitset::npos ? position : position & ~Bitset::BlockMask)
        {
            if (position != Bitset::npos)
                m_remaining = bits.blocks()[Bitset::block_index(position)] & (Bitset::full_mask() << Bitset::block_pos(position));
        }
        std::size_t operator*() const noexcept { return m_word_offset + static_cast<std::size_t>(std::countr_zero(m_remaining)); }
        Iterator& operator++() noexcept
        {
            m_remaining &= m_remaining - 1;
            if (m_remaining != 0)
                return *this;

            const auto blocks = m_bits.blocks();
            for (std::size_t block = Bitset::block_index(m_word_offset) + 1; block < blocks.size(); ++block)
            {
                m_remaining = blocks[block];
                if (m_remaining != 0)
                {
                    m_word_offset = block * Bitset::Digits;
                    return *this;
                }
            }
            m_word_offset = Bitset::npos;
            return *this;
        }
        Iterator operator++(int) noexcept
        {
            auto result = *this;
            ++*this;
            return result;
        }
        friend bool operator==(const Iterator& iterator, std::default_sentinel_t) noexcept { return iterator.m_word_offset == Bitset::npos; }
        friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept
        {
            return lhs.m_bits.data() == rhs.m_bits.data() && lhs.m_bits.size() == rhs.m_bits.size() && lhs.m_word_offset == rhs.m_word_offset
                   && lhs.m_remaining == rhs.m_remaining;
        }
    };

    SetBitIndices() noexcept = default;
    explicit SetBitIndices(Bitset bits) noexcept : m_bits(bits) {}
    [[nodiscard]] Iterator begin() const noexcept { return Iterator(m_bits, m_bits.find_first()); }
    [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }
};

template<std::unsigned_integral Block>
auto set_bit_indices(BitsetSpan<Block> bits) noexcept
{ return SetBitIndices<std::remove_const_t<Block>>(BitsetSpan<const std::remove_const_t<Block>>(bits)); }

/// Equal bits; both spans keep their trailing bits clear.
template<std::unsigned_integral B1, std::unsigned_integral B2>
    requires SameAsIgnoringConst<B1, B2>
constexpr bool operator==(const BitsetSpan<B1>& lhs, const BitsetSpan<B2>& rhs) noexcept
{
    assert(lhs.trailing_bits_zero());
    assert(rhs.trailing_bits_zero());
    if (lhs.num_bits() != rhs.num_bits())
        return false;
    const std::size_t n = BitsetSpan<std::remove_const_t<B1>>::num_blocks(lhs.num_bits());
    const auto lb = lhs.blocks();
    const auto rb = rhs.blocks();
    for (std::size_t i = 0; i < n; ++i)
        if (lb[i] != rb[i])
            return false;
    return true;
}

/// Calls `callback` with the position of every bit set in the blocks `combiner` makes of the spans' blocks, one block
/// of each at a time (the last block masked to the size); the spans have the same size, else std::invalid_argument.
template<typename Callback, typename BlockCombiner, std::unsigned_integral Block0, std::unsigned_integral... Blocks>
    requires(SameAsIgnoringConst<Block0, Blocks> && ...)
void for_each_bit(Callback callback, BlockCombiner combiner, const BitsetSpan<Block0>& first, const BitsetSpan<Blocks>&... rest)
{
    using U = std::remove_const_t<Block0>;
    const std::size_t num_bits = first.num_bits();
    if (!((rest.num_bits() == num_bits) && ...))
        throw std::invalid_argument("BitsetSpan operands must have the same size.");
    assert(first.trailing_bits_zero());
    assert(((rest.trailing_bits_zero()) && ...));
    const auto fb = first.blocks();
    const auto rb = std::tuple { rest.blocks()... };
    const std::size_t n = fb.size();
    const U last = BitsetSpan<const U>::last_mask(num_bits);
    std::size_t offset = 0;
    for (std::size_t block = 0; block < n; ++block)
    {
        U w = std::apply([&](const auto&... b) noexcept -> U { return static_cast<U>(combiner(fb[block], b[block]...)); }, rb);
        if (block + 1 == n)
            w &= last;
        bit::for_each_set_bit(w, [&](std::size_t at) { callback(offset + at); });
        offset += BitsetSpan<const U>::Digits;
    }
}

}  // namespace urd::repr

namespace urd
{

/// A span hashes by its size and its blocks (the trailing bits clear).
template<std::unsigned_integral Block>
struct Hash<repr::BitsetSpan<Block>>
{
    hash_t operator()(const repr::BitsetSpan<Block>& bitset_span) const noexcept
    {
        hash_t aggregated_hash = bitset_span.num_bits();
        for (const auto block : bitset_span.blocks())
            urd::hash_combine(aggregated_hash, block);
        return aggregated_hash;
    }
};

}  // namespace urd

namespace std::ranges
{
template<std::unsigned_integral Block>
inline constexpr bool enable_borrowed_range<urd::repr::SetBitIndices<Block>> = true;
}
