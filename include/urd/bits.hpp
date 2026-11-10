#pragma once

// urd's bit vocabulary, for code outside urd: a span over the words of a bitset, its set bits as a range, and the
// helpers that read and write integers packed into words. These are the names to use; urd::repr is urd's own. (The
// tag urd::bit is a request's; the helpers are urd::bits.)

#include "urd/repr/containers/bit.hpp"
#include "urd/repr/containers/bitset_span.hpp"

namespace urd
{

/// A span over the words of a bitset, a bit per index below its length.
template<typename Block>
using bitset_span = repr::BitsetSpan<Block>;
/// The set bits of a bitset_span, as a range of their indices: what set_bit_indices gives.
template<typename Block>
using set_bit_indices_view = repr::SetBitIndices<Block>;
using repr::for_each_bit;
using repr::set_bit_indices;

namespace bits
{

using repr::bit::bits_needed;
using repr::bit::bits_per_block_v;
using repr::bit::ceil_div;
using repr::bit::for_each_set_bit;
using repr::bit::int_reference;
using repr::bit::lo_set;
using repr::bit::read_int;
using repr::bit::write_int;

}  // namespace bits

}  // namespace urd
