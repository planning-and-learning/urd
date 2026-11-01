// A kind of Store defined outside urd, with a category, a facet and a container of its own: a tally, which counts
// how often each value was added. Nothing in include/urd/ names any of it; a request names it (tally<T>), and its
// container plugs in through urd::detail::declarations_of, urd::detail::representation_of, urd::include, urd::detail::facet_condition and
// urd::detail::facet_operations; its operations are its own (add, count), costed by its container.

#include "urd/costs.hpp"
#include "urd/kinds.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

// The category: does the Store count its values?
namespace
{
namespace tally
{

enum class tallying : std::uint8_t
{
    none,
    tallied,
};
struct any_tallying : urd::tag_defaults
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
}  // namespace

template<>
struct urd::tags_of<tally::tallying>
{
    using type = detail::type_list<tally::any_tallying, tally::tallied>;
};

namespace
{
namespace tally
{

/// Its operation `add`: adds a value once more; a container states its cost by an entry for it (urd::cost_entry).
struct add
{
    static constexpr std::string_view name = "add";
    static constexpr std::string_view description = "adds a value once more";
};
/// Its operation `count`: how often a value was added; a container states its cost by an entry for it.
struct count
{
    static constexpr std::string_view name = "count";
    static constexpr std::string_view description = "how often a value was added";
};

/// The facet a tally offers.
template<typename Derived, typename R, typename S>
class counting;

/// The kind: values added and counted, never iterated. It states what differs from any kind: its question, its facet,
/// and what a request of it means (it tallies).
struct tally_kind : urd::detail::any_kind
{
    static constexpr std::string_view name = "tally";
    using format = urd::detail::request_format<tallying>;
    using facets = urd::detail::facet_list<counting>;
    static constexpr urd::detail::property_set meaning(urd::detail::property_set stated) { return any_kind::meaning(stated.put(tallying::tallied)); }
};

/// What a request names: a tally of values of type T, as urd's kinds name theirs (urd::detail::values_of).
template<typename T>
struct tally : urd::detail::values_of<tally_kind, T>
{
};

}  // namespace tally
}  // namespace

#include "urd/store.hpp"

#include <gtest/gtest.h>

namespace
{
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
    static constexpr urd::detail::property_set relies_on {};
    static constexpr std::array<urd::tag, 0> gives_elements {};
    using operation_costs = urd::cost_table<urd::cost_entry<add, urd::costs::one_ae>, urd::cost_entry<count, urd::costs::one_e>>;
    static constexpr double words = 3;
    static constexpr std::string_view description = "a count per value";
    static constexpr std::string_view holds = "a hash table of counts";
};

}  // namespace tally
}  // namespace

template<>
struct urd::detail::facet_condition<tally::counting>
{
    template<typename R, typename S>
    static constexpr bool on()
    { return true; }
};
template<>
struct urd::detail::facet_operations<tally::counting>
{
    using offered = urd::operation_list<tally::add, tally::count>;
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

using Words = urd::Store<tally::tally<std::string>>;

static_assert(std::same_as<Words::representation, tally::Tally<std::string>>);
static_assert(std::same_as<urd::container_traits<Words>::kind, tally::tally_kind>);
// Its own operations, with the costs its container states.
static_assert(urd::cost_of<tally::add, tally::tally_decl::operation_costs>().has_value()
              && urd::cost_of<tally::count, tally::tally_decl::operation_costs>().has_value());
static_assert(!urd::cost_of<tally::add, urd::detail::vector_decl::operation_costs>().has_value());  // urd's containers state no cost for it
// What tools read of the kind (urd-search): its operations' words, how it keeps its elements, its memory.
static_assert(urd::operation_v<tally::add>->name == "add" && !urd::operation_v<tally::count>->description.empty());
static_assert(!urd::container_traits<Words>::iterable && !urd::has<Words, urd::positional>);
// Every kind's questions are its own too (threading).
static_assert(urd::detail::in_format(urd::detail::format_of<tally::tally_kind> {},
                                     urd::detail::property_set::of({ tally::tallying::tallied, urd::threading::concurrent })));
static_assert(tally::tally_decl::words == 3);
// urd's own kinds are untouched by the new one.
static_assert(std::same_as<urd::Store<urd::sequence<int>>::representation, std::vector<int>>);

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
