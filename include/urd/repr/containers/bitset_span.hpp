#pragma once

// Copied from yggdrasil (include/yggdrasil/containers/dynamic_bitset.hpp): BitsetSpan, a bitset over blocks it does
// not own, and SetBitIndices, its set bits as a range; names moved from ygg to urd::repr. yggdrasil's adapters for
// boost::dynamic_bitset and the multi-bitset combinators are not copied. Casts added where urd's -Wsign-conversion
// asks for them.

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
#include <type_traits>

namespace urd::repr
{

/// Copied from yggdrasil (include/yggdrasil/core/concepts.hpp): T and U are the same type but for const.
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

    class bit_reference
    {
    private:
        U* m_block;
        U m_mask;

        bit_reference(U& block, U mask) noexcept : m_block(&block), m_mask(mask) {}

        friend class BitsetSpan;

    public:
        bit_reference(const bit_reference&) noexcept = default;

        operator bool() const noexcept { return (*m_block & m_mask) != U { 0 }; }

        bit_reference& operator=(bool value) noexcept
        {
            if (value)
                *m_block |= m_mask;
            else
                *m_block &= ~m_mask;

            return *this;
        }

        ~bit_reference() = default;
        bit_reference& operator=(const bit_reference& other) noexcept
        {
            if (this != &other)
                *this = static_cast<bool>(other);
            return *this;
        }
        bit_reference(bit_reference&&) noexcept = default;
        bit_reference& operator=(bit_reference&& other) noexcept { return *this = static_cast<const bit_reference&>(other); }

        bit_reference& flip() noexcept
        {
            *m_block ^= m_mask;
            return *this;
        }
    };

    static constexpr std::size_t Digits = std::numeric_limits<U>::digits;
    static constexpr std::size_t BlockShift = std::countr_zero(Digits);
    static constexpr std::size_t BlockMask = Digits - 1;

    static constexpr std::size_t block_index(std::size_t pos) noexcept { return pos >> BlockShift; }
    static constexpr std::size_t block_pos(std::size_t pos) noexcept { return pos & BlockMask; }
    static constexpr std::size_t num_blocks(std::size_t num_bits) noexcept { return (num_bits + Digits - 1) >> BlockShift; }

    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    static constexpr U full_mask() noexcept { return ~U { 0 }; }

    static constexpr U last_mask(std::size_t num_bits) noexcept
    {
        const std::size_t r = num_bits % Digits;
        return (r == 0) ? full_mask() : (U { 1 } << r) - U { 1 };
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

        return bit_reference(blocks()[block_index(pos)], U { 1 } << block_pos(pos));
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
            result += std::popcount(static_cast<U>(blocks()[i] & other.blocks()[i]));
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

        blocks()[block_index(pos)] |= (U { 1 } << block_pos(pos));
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

        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
        {
            U w = blocks()[i];
            if (w == U { 0 })
                continue;

            const std::size_t bit = i * Digits + static_cast<std::size_t>(std::countr_zero(w));
            return bit < m_num_bits ? bit : npos;
        }

        return npos;
    }

    [[nodiscard]] std::size_t find_next(std::size_t pos) const noexcept
    {
        assert(trailing_bits_zero());

        if (pos >= m_num_bits)
            return npos;
        ++pos;
        if (pos >= m_num_bits)
            return npos;

        std::size_t i = block_index(pos);
        U w = blocks()[i] & (~U { 0 } << block_pos(pos));

        const std::size_t n = num_blocks(m_num_bits);
        for (;;)
        {
            if (w != U { 0 })
            {
                const std::size_t bit = i * Digits + static_cast<std::size_t>(std::countr_zero(w));
                return bit < m_num_bits ? bit : npos;
            }

            if (++i == n)
                return npos;

            w = blocks()[i];
        }
    }

    [[nodiscard]] std::size_t find_first_zero() const noexcept
    {
        const std::size_t n = num_blocks(m_num_bits);
        for (std::size_t i = 0; i < n; ++i)
        {
            U w = ~blocks()[i];

            if (i == n - 1)
                w &= last_mask(m_num_bits);

            if (w == U { 0 })
                continue;

            const std::size_t bit = i * Digits + static_cast<std::size_t>(std::countr_zero(w));
            return bit < m_num_bits ? bit : npos;
        }

        return npos;
    }

    [[nodiscard]] std::size_t find_next_zero(std::size_t pos) const noexcept
    {
        if (pos >= m_num_bits)
            return npos;
        ++pos;
        if (pos >= m_num_bits)
            return npos;

        std::size_t i = block_index(pos);
        U w = ~blocks()[i] & (~U { 0 } << block_pos(pos));

        const std::size_t n = num_blocks(m_num_bits);
        for (;;)
        {
            if (i == n - 1)
                w &= last_mask(m_num_bits);

            if (w != U { 0 })
            {
                const std::size_t bit = i * Digits + static_cast<std::size_t>(std::countr_zero(w));
                return bit < m_num_bits ? bit : npos;
            }

            if (++i == n)
                return npos;

            w = ~blocks()[i];
        }
    }

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

}  // namespace urd::repr

namespace std::ranges
{
template<std::unsigned_integral Block>
inline constexpr bool enable_borrowed_range<urd::repr::SetBitIndices<Block>> = true;
}
