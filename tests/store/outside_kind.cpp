// A kind of Store defined outside urd, with a category, a facet and a container of its own: a tally, which counts
// how often each value was added. Nothing in include/urd/ names any of it; it plugs in through urd::extra_kinds,
// urd::detail::declarations_of, urd::detail::representation_of, urd::include and urd::detail::facet_condition.

#include "urd/kinds.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

// The category: does the Store count its values?
namespace tally
{

enum class tallying : std::uint8_t
{
    none,
    tallied,
};
struct any_tallying
{
    using root = any_tallying;
    static constexpr auto value = tallying::none;
    static constexpr std::string_view name = "any_tallying";
};
struct tallied : any_tallying
{
    static constexpr auto value = tallying::tallied;
    static constexpr std::string_view name = "tallied";
};

}  // namespace tally

template<>
struct urd::tags_of<tally::tallying>
{
    using type = detail::type_list<tally::any_tallying, tally::tallied>;
};

namespace tally
{

/// The facet a tally offers.
template<typename Derived, typename R, typename S>
class counting;

/// The kind: values added and counted, never iterated.
struct tally_kind
{
    static constexpr std::string_view name = "tally";
    using format = urd::detail::request_format<tallying, urd::threading>;
    template<typename K, typename V>
    using element_type = V;
    template<typename K, typename V>
    using mapped_type = void;
    static constexpr bool iterable = false;
    using facets = urd::detail::facet_list<counting>;
    static constexpr bool keeps_by_value = false;
    static constexpr bool positional = false;
    static constexpr bool claims(const urd::detail::property_set& stated, bool /*keyed*/, bool /*valued*/) { return stated.has(tallying::tallied); }
    static constexpr urd::detail::property_set meaning(urd::detail::property_set stated) { return stated; }
};

}  // namespace tally

template<>
struct urd::extra_kinds<void>
{
    using type = detail::type_list<tally::tally_kind>;
};

#include "urd/store.hpp"

#include <gtest/gtest.h>

namespace tally
{

/// The container: a count per value.
template<typename T>
class Tally
{
public:
    void add(const T& value) { ++m_counts[value]; }
    [[nodiscard]] std::size_t count(const T& value) const
    {
        const auto found = m_counts.find(value);
        return found == m_counts.end() ? 0 : found->second;
    }

private:
    std::unordered_map<T, std::size_t> m_counts;
};

template<typename Derived, typename R, typename S>
class counting
{
public:
    void add(const typename S::element_type& value) { urd::detail::store_access::representation(static_cast<Derived&>(*this)).add(value); }
    [[nodiscard]] std::size_t count(const typename S::element_type& value) const
    { return urd::detail::store_access::representation(static_cast<const Derived&>(*this)).count(value); }
};

/// Its declaration, as urd's containers declare theirs.
struct tally_decl
{
    static constexpr std::string_view id = "tally";
    static constexpr auto headers = urd::detail::header_list<1> { "tests/store/outside_kind.cpp" };
    using kind = tally_kind;
    static constexpr urd::detail::property_set provides = urd::detail::property_set::of({ tallying::tallied });
    static constexpr urd::detail::role_tags relies_on {};
    static constexpr std::array<urd::tag, 0> gives_elements {};
    struct operation_costs
    {
        static constexpr urd::cost insert = urd::costs::one_a;
        static constexpr double words = 3;
    };
    static constexpr std::string_view description = "a count per value";
    static constexpr std::string_view holds = "a hash table of counts";
};

}  // namespace tally

template<>
struct urd::detail::facet_condition<tally::counting>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return true; }
};
template<>
struct urd::detail::declarations_of<tally::tally_kind>
{
    using type = type_list<tally::tally_decl>;
};
template<typename S>
struct urd::detail::representation_of<tally::tally_decl, S>
{
    using type = tally::Tally<typename S::element_type>;
};
template<>
struct urd::include<"tests/store/outside_kind.cpp">
{
};

namespace
{

using Words = urd::Store<urd::value<std::string>, tally::tallied>;

static_assert(std::same_as<Words::representation, tally::Tally<std::string>>);
static_assert(std::same_as<urd::container_traits<Words>::kind, tally::tally_kind>);
// urd's own kinds are untouched by the new one.
static_assert(std::same_as<urd::Store<urd::value<int>>::representation, std::vector<int>>);

TEST(OutsideKind, ResolvesAndOffersItsFacet)
{
    auto words = Words();
    words.add("a");
    words.add("b");
    words.add("a");
    EXPECT_EQ(words.count("a"), 2U);
    EXPECT_EQ(words.count("b"), 1U);
    EXPECT_EQ(words.count("c"), 0U);
}

}  // namespace
