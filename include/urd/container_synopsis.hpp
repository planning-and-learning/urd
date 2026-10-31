#pragma once

// What a representation is, in words: declared beside its traits and costs, in the header that declares its
// container_traits (urd/repr/*_fwd.hpp and the like), which names it without defining it. The docs, the generated
// tables (share/urd/complexity.json, benchmarks/requests.json, docs/RESOLUTION.md) and the tests' names all read these;
// downstream code never needs them, as it uses the Store and never names the representation.

#include <array>
#include <cstddef>
#include <string_view>

namespace urd
{

/// What representation R is: `id`, a short stable name (`unordered_flat_set`); `description`, what it is
/// (`boost::unordered_flat_set (open-addressing hash table)`); and `holds`, what it keeps besides its elements, where
/// its memory goes. Declared for every registered representation; none here.
template<typename R>
struct container_synopsis
{
};

/// The strings Parts, one after the other, as one string with static storage: a wrapper's id is its prefix and its
/// part's id (`layered_` and `record_interner`), so a wrapper over a new part has an id of its own.
template<const std::string_view&... Parts>
struct joined
{
    static constexpr auto storage = []
    {
        auto result = std::array<char, (Parts.size() + ... + 0)> {};
        std::size_t at = 0;
        (
            (void) [&] {
                for (const char each : Parts)
                    result[at++] = each;
            }(),
            ...);
        return result;
    }();
    static constexpr auto value = std::string_view(storage.data(), storage.size());
};

}  // namespace urd
