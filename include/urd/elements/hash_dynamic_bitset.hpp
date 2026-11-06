#pragma once

// urd::Hash for boost::dynamic_bitset, opt-in: urd does not depend on Boost.DynamicBitset; include this where a
// dynamic_bitset is hashed.

#include "urd/elements/hash.hpp"

#include <boost/dynamic_bitset.hpp>
#include <boost/iterator/function_output_iterator.hpp>

namespace urd
{

/// A dynamic_bitset hashes by its size and its blocks.
template<typename Block, typename Allocator>
struct Hash<boost::dynamic_bitset<Block, Allocator>>
{
    hash_t operator()(const boost::dynamic_bitset<Block, Allocator>& bitset) const
    {
        hash_t seed = bitset.size();
        boost::to_block_range(bitset, boost::make_function_output_iterator([&seed](const Block block) noexcept { urd::hash_combine(seed, block); }));
        return seed;
    }
};

}  // namespace urd
