#pragma once

// Bit manipulation for containers that store values in bits: masks, set-bit iteration, unsigned integers packed across
// blocks (plain, or through relaxed atomics where threads read while one writes), and references to a packed integer
// or a bit. Moved from yggdrasil (include/yggdrasil/core/bit.hpp and atomic_bit.hpp); partially taken from sdsl-lite
// (https://github.com/simongog/sdsl-lite/blob/master/include/sdsl/bits.hpp).

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace urd::repr::bit
{

template<std::unsigned_integral T>
constexpr T ceil_div(T x, T y)
{
    assert(y > 0);
    return x == 0 ? 0 : T(1) + (x - 1) / y;
}

/// The bits a value below `domain_size` needs (at least one).
template<std::unsigned_integral T>
constexpr T bits_needed(T domain_size)
{
    if (domain_size <= 1)
        return 1;
    return std::bit_width(static_cast<T>(domain_size - 1));
}

template<std::unsigned_integral Block>
inline constexpr std::size_t bits_per_block_v = std::numeric_limits<Block>::digits;

/// lo_set<Block>[i]: a Block with the i least significant bits set (lo_set[0] = 0, lo_set[1] = 1, lo_set[2] = 3, ...).
template<std::unsigned_integral Block>
consteval auto make_lo_set()
{
    constexpr std::size_t digits = std::numeric_limits<Block>::digits;
    std::array<Block, digits + 1> table {};
    table[0] = Block { 0 };
    for (std::size_t i = 1; i < digits; ++i)
        table[i] = static_cast<Block>((Block { 1 } << i) - Block { 1 });
    table[digits] = static_cast<Block>(~Block { 0 });
    return table;
}

template<std::unsigned_integral Block>
inline constexpr auto lo_set = make_lo_set<Block>();

/// Whether x fits in len bits.
template<std::unsigned_integral Block>
constexpr bool fits(Block x, uint8_t len) noexcept
{ return (x & ~lo_set<Block>[len]) == 0; }

/// The Blocks that `bits` bits of packed integers take: those they fill and one of room, as a packed integer is read and
/// written through the Block it starts in and the next.
template<std::unsigned_integral Block>
constexpr std::size_t packed_blocks(std::size_t bits) noexcept
{ return ceil_div<std::size_t>(bits, bits_per_block_v<Block>) + 1; }

/// Calls callback(i) for each set bit i of the word, in ascending order.
template<std::unsigned_integral Block, typename Callback>
constexpr void for_each_set_bit(Block word, Callback callback)
{
    for (; word; word &= word - 1)
        callback(static_cast<std::size_t>(std::countr_zero(word)));
}

/// Writes the len-bit unsigned integer x at bit offset of words[0]; a value that crosses into the next block needs
/// words[1] too (asserted in Debug). The bits around it are kept.
template<std::unsigned_integral Block>
constexpr void write_int(std::type_identity_t<std::span<Block>> words, Block x, uint8_t offset, const uint8_t len)
{
    constexpr std::size_t digits = std::numeric_limits<Block>::digits;

    static_assert(std::has_single_bit(digits), "Block width must be a power of two.");
    assert(len <= digits && "Width exceeds block width.");
    assert(offset < digits && "Offset must lie within one block.");

    constexpr Block all_set = static_cast<Block>(-1);

    x &= lo_set<Block>[len];
    if (offset + len >= digits)  // >= avoids shifting all_set by exactly digits below
    {
        words[0] &= ((lo_set<Block>[offset]));
        words[0] |= (x << offset);
        offset = (offset + len) & (digits - 1);  // bits written into words[1]; zero on an exact boundary
        if (offset != 0)
        {
            words[1] &= static_cast<Block>(~lo_set<Block>[offset]);
            words[1] |= (x >> (len - offset));
        }
    }
    else
    {
        words[0] &= static_cast<Block>((all_set << (offset + len)) | lo_set<Block>[offset]);
        words[0] |= (x << offset);
    }
}

/// Reads the len-bit unsigned integer at bit offset of words[0] (and words[1], where it crosses into it).
template<std::unsigned_integral Block>
constexpr Block read_int(std::type_identity_t<std::span<const Block>> words, uint8_t offset, const uint8_t len)
{
    constexpr std::size_t digits = std::numeric_limits<Block>::digits;

    static_assert(std::has_single_bit(digits), "Block width must be a power of two.");
    assert(len <= digits && "Width exceeds block width.");
    assert(offset < digits && "Offset must lie within one block.");

    const Block w1 = words[0] >> offset;

    if (offset + len > digits)
        return w1 | ((words[1] & lo_set<Block>[(offset + len) & (digits - 1)]) << (digits - offset));
    else
        return w1 & lo_set<Block>[len];
}

/// A block read with a relaxed atomic load: what readers do while a writer changes other bits of the same block.
template<std::unsigned_integral Block>
Block atomic_load(const Block* block) noexcept
{ return std::atomic_ref<Block>(*const_cast<Block*>(block)).load(std::memory_order_relaxed); }

/// Replaces the bits of mask in the block by those of value, atomically (relaxed): the other bits stay as any other
/// writer leaves them.
template<std::unsigned_integral Block>
void atomic_replace_bits(Block* block, Block mask, Block value) noexcept
{
    auto reference = std::atomic_ref<Block>(*block);
    auto current = reference.load(std::memory_order_relaxed);
    const auto masked_value = static_cast<Block>(value & mask);
    while ((current & mask) != masked_value
           && !reference.compare_exchange_weak(current, static_cast<Block>((current & ~mask) | masked_value), std::memory_order_relaxed))
    {
    }
}

/// read_int through relaxed atomic loads. A value spanning two blocks is not read as one atomic unit.
template<std::unsigned_integral Block>
Block atomic_read_int(std::type_identity_t<std::span<const Block>> words, uint8_t offset, uint8_t len) noexcept
{
    constexpr std::size_t digits = std::numeric_limits<Block>::digits;
    static_assert(std::has_single_bit(digits), "Block width must be a power of two.");
    assert(len <= digits && "Width exceeds block width.");
    assert(offset < digits && "Offset must lie within one block.");
    const Block first = atomic_load(&words[0]) >> offset;
    if (offset + len > digits)
        return first | ((atomic_load(&words[1]) & lo_set<Block>[(offset + len) & (digits - 1)]) << (digits - offset));
    return first & lo_set<Block>[len];
}

/// write_int through relaxed atomic replacements of bits. A value spanning two blocks is not written as one unit.
template<std::unsigned_integral Block>
void atomic_write_int(std::type_identity_t<std::span<Block>> words, Block value, uint8_t offset, uint8_t len) noexcept
{
    constexpr std::size_t digits = std::numeric_limits<Block>::digits;
    static_assert(std::has_single_bit(digits), "Block width must be a power of two.");
    assert(len <= digits && "Width exceeds block width.");
    assert(offset < digits && "Offset must lie within one block.");
    value &= lo_set<Block>[len];
    const auto first_len = static_cast<uint8_t>(std::min<size_t>(len, digits - offset));
    const auto first_mask = static_cast<Block>(lo_set<Block>[first_len] << offset);
    atomic_replace_bits(&words[0], first_mask, static_cast<Block>(value << offset));
    if (first_len < len)
    {
        const auto second_len = static_cast<uint8_t>(len - first_len);
        atomic_replace_bits(&words[1], lo_set<Block>[second_len], static_cast<Block>(value >> first_len));
    }
}

/// The packed integer at a bit: read and assigned as a Block, read_int and write_int; one that does not fit in its
/// width is rejected (std::out_of_range).
template<std::unsigned_integral Block>
class int_reference
{
public:
    using value_type = Block;

    constexpr int_reference(std::span<Block> words, uint8_t offset, uint8_t len) noexcept : m_words(words), m_offset(offset), m_len(len) {}

    constexpr int_reference& operator=(const value_type& value)
    {
        if ((value & ~lo_set<Block>[m_len]) != 0)
            throw std::out_of_range("urd::repr::bit::int_reference: the value exceeds the bit width");
        write_int<Block>(m_words, value, m_offset, m_len);
        return *this;
    }
    constexpr int_reference(const int_reference&) = default;
    constexpr ~int_reference() = default;
    constexpr int_reference& operator=(const int_reference& other)
    {
        if (this != &other)
            *this = static_cast<value_type>(other);
        return *this;
    }
    constexpr int_reference(int_reference&&) = default;
    constexpr int_reference& operator=(int_reference&& other) noexcept { return *this = static_cast<const int_reference&>(other); }

    constexpr operator value_type() const { return read_int<Block>(m_words, m_offset, m_len); }  // NOLINT(google-explicit-constructor)

private:
    std::span<Block> m_words;
    uint8_t m_offset;
    uint8_t m_len;
};

/// int_reference through relaxed atomic block operations: race-free where threads share the blocks. A value spanning
/// two blocks is not read or written as one atomic unit.
template<std::unsigned_integral Block>
class atomic_int_reference
{
public:
    using value_type = Block;

    atomic_int_reference(std::span<Block> words, uint8_t offset, uint8_t len) noexcept : m_words(words), m_offset(offset), m_len(len)
    {
        constexpr std::size_t digits = std::numeric_limits<Block>::digits;
        static_assert(std::has_single_bit(digits), "Block width must be a power of two.");
        assert(len <= digits && "Width exceeds block width.");
        assert(offset < digits && "Offset must lie within one block.");
    }
    atomic_int_reference& operator=(const value_type& value)
    {
        if ((value & ~lo_set<Block>[m_len]) != 0)
            throw std::out_of_range("urd::repr::bit::atomic_int_reference: the value exceeds the bit width");
        atomic_write_int<Block>(m_words, value, m_offset, m_len);
        return *this;
    }
    atomic_int_reference(const atomic_int_reference&) = default;
    ~atomic_int_reference() = default;
    atomic_int_reference& operator=(const atomic_int_reference& other)
    {
        if (this != &other)
            *this = static_cast<value_type>(other);
        return *this;
    }
    atomic_int_reference(atomic_int_reference&&) = default;
    atomic_int_reference& operator=(atomic_int_reference&& other) noexcept { return *this = static_cast<const atomic_int_reference&>(other); }

    operator value_type() const { return atomic_read_int<Block>(m_words, m_offset, m_len); }  // NOLINT(google-explicit-constructor)

private:
    std::span<Block> m_words;
    uint8_t m_offset;
    uint8_t m_len;
};

/// The bit at an index of blocks: read and assigned as a bool.
template<std::unsigned_integral Block>
struct bit_reference
{
    std::span<Block> data;
    size_t bit;

    bit_reference(std::span<Block> data_, size_t bit_) noexcept : data(data_), bit(bit_) {}

    static constexpr size_t bits_per_block = std::numeric_limits<Block>::digits;

    static constexpr size_t block_index(size_t bit) { return bit / bits_per_block; }
    static constexpr size_t bit_index(size_t bit) { return bit % bits_per_block; }

    bit_reference& operator=(bool value)
    {
        Block& block = data[block_index(bit)];
        const Block mask = Block(1) << bit_index(bit);

        if (value)
            block |= mask;
        else
            block &= ~mask;

        return *this;
    }

    bit_reference& flip() noexcept
    {
        data[block_index(bit)] ^= Block(1) << bit_index(bit);
        return *this;
    }

    bit_reference(const bit_reference&) = default;
    ~bit_reference() = default;
    bit_reference& operator=(const bit_reference& other) noexcept
    {
        if (this != &other)
            *this = static_cast<bool>(other);
        return *this;
    }
    bit_reference(bit_reference&&) noexcept = default;
    bit_reference& operator=(bit_reference&& other) noexcept { return *this = static_cast<const bit_reference&>(other); }

    operator bool() const noexcept { return ((data[block_index(bit)] >> bit_index(bit)) & Block(1)) != 0; }  // NOLINT(google-explicit-constructor)
};

}  // namespace urd::repr::bit
