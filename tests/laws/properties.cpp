// The property suite (tests/laws/properties.hpp) for what is no registered container: a container from elsewhere through
// an adapter, and two false claims the suite must catch. Every registered container is checked by its own test
// objects (tests/laws/container.cpp).

#include "properties.hpp"

#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/sparse_set.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace lib
{

/// A library's append-only journal, with its own names. Entries never move once written.
class Journal
{
public:
    [[nodiscard]] auto begin() const { return m_entries.begin(); }
    [[nodiscard]] auto end() const { return m_entries.end(); }
    [[nodiscard]] std::size_t count() const { return m_entries.size(); }

    const int& append(int entry) { return m_entries.emplace_back(entry); }

private:
    std::deque<int> m_entries;
};

}  // namespace lib
}  // namespace

namespace
{

/// A container outside the registry, declared as urd's are, so that its claims are checked like theirs: the kind it
/// serves, what it provides, what it relies on, and the roles its element types give.
template<typename Kind, urd::detail::property_set Provides, urd::detail::property_set Relies = urd::detail::property_set {}>
struct test_declaration
{
    static constexpr std::string_view id = "test container";
    static constexpr auto headers = urd::detail::header_list<1> { "tests/laws/properties.cpp" };
    using kind = Kind;
    static constexpr urd::detail::property_set provides = Provides;
    static constexpr urd::detail::property_set relies_on = Relies;
    static constexpr std::array<urd::tag, 0> gives_elements {};
    using operation_costs = urd::cost_table<>;
    static constexpr double words = 1;
    static constexpr std::string_view description = "a container a test declares";
    static constexpr std::string_view holds = "its elements";
};

}  // namespace

namespace
{

/// An adapter: the library's journal under the names a sequence that only grows must offer. It only renames.
class Journal
{
public:
    Journal() = default;
    template<std::input_iterator InputIt>
    Journal(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](int entry) { m_journal.append(entry); });
    }

    [[nodiscard]] auto begin() const { return m_journal.begin(); }
    [[nodiscard]] auto end() const { return m_journal.end(); }
    [[nodiscard]] std::size_t size() const { return m_journal.count(); }

    void push_back(int entry) { m_journal.append(entry); }
    const int& emplace_back(int entry) { return m_journal.append(entry); }

private:
    lib::Journal m_journal;
};

}  // namespace

// What it guarantees: entries keep their address; and what it relies on: it only grows.
template<>
struct urd::detail::declaration_of<Journal>
{
    using declaration = test_declaration<urd::sequence_kind,
                                         urd::detail::property_set::of({ urd::position::positional, urd::referencing::stable_refs }),
                                         urd::detail::property_set::of({ urd::change::monotone })>;
    static constexpr urd::detail::property_set roles = urd::detail::roles_of<urd::sequence<int>>();
};

INSTANTIATE_TYPED_TEST_SUITE_P(Adapters, Properties, testing::Types<Journal>, urd::test::Names);

namespace
{

/// A vector that wrongly claims its elements never move.
struct ClaimsStableRefs
{
    std::vector<int> elements;

    ClaimsStableRefs() = default;
    template<std::input_iterator InputIt>
    ClaimsStableRefs(InputIt first, InputIt last) : elements(first, last)
    {
    }
    [[nodiscard]] auto begin() const { return elements.begin(); }
    [[nodiscard]] auto end() const { return elements.end(); }
    [[nodiscard]] std::size_t size() const { return elements.size(); }
    void push_back(int element) { elements.push_back(element); }
    const int& emplace_back(int element) { return elements.emplace_back(element); }
};

}  // namespace

template<>
struct urd::detail::declaration_of<ClaimsStableRefs>
{
    using declaration = test_declaration<urd::sequence_kind,
                                         urd::detail::property_set::of({ urd::referencing::stable_refs }),
                                         urd::detail::property_set::of({ urd::change::monotone })>;
    static constexpr urd::detail::property_set roles = urd::detail::roles_of<urd::sequence<int>>();
};

TEST(Properties, AFalseClaimIsCaught)
{
    const auto found = urd::verify(ClaimsStableRefs(), 31).value_or("");
    EXPECT_NE(found.find("stable_refs"), std::string::npos) << found;
}

namespace
{

/// A dense map whose slots lie in a vector, wrongly claiming its values never move. Its iteration builds each
/// entry on the fly, so only a check through the values (not the entries) catches it.
struct ClaimsStableValues : urd::repr::IndexMap<int, int, false>
{
    using IndexMap::IndexMap;
};

}  // namespace

template<>
struct urd::detail::declaration_of<ClaimsStableValues>
{
    using declaration =
        test_declaration<urd::map_kind,
                         urd::detail::property_set::of(
                             { urd::order::sorted, urd::uniqueness::unique, urd::keys::dense, urd::referencing::stable_refs, urd::memory::retains })>;
    static constexpr urd::detail::property_set roles = urd::detail::roles_of<urd::map<int, int>>();
};

TEST(Properties, AFalseClaimAboutValuesIsCaught)
{
    const auto found = urd::verify(ClaimsStableValues(), 31).value_or("");
    EXPECT_NE(found.find("stable_refs"), std::string::npos) << found;
}
