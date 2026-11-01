#pragma once

// Element types of the request grid (tests/grid.hpp) beside the standard ones: a key identified by one integral member,
// and a record with text; each spelled as a request names it (urd::type_spelling).

#include "urd/flat.hpp"
#include "urd/store.hpp"

#include <cstdint>

namespace urd::test
{

/// A key identified by one integral member: hashable, orderable and indexable.
struct Id
{
    std::uint32_t id = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Id::id> {}; }
};

/// A record with text: serialized into the arena where stored flat.
struct Text
{
    urd::flat_string text;
    static constexpr auto identifying_members() noexcept { return urd::Members<&Text::text> {}; }
    static constexpr auto cista_members() noexcept { return urd::Members<&Text::text> {}; }
};

}  // namespace urd::test

#include "urd/format.hpp"

#include <string>

template<>
struct urd::type_spelling<urd::test::Id>
{
    static std::string spelled() { return "Id"; }
};
template<>
struct urd::type_spelling<urd::test::Text>
{
    static std::string spelled() { return "Text"; }
};
