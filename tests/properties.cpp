// The property suite (tests/properties.hpp) for what is no registered container: a container from elsewhere through
// an adapter, and two false claims the suite must catch. Every registered container is checked by its own test
// objects (tests/container.cpp).

#include "properties.hpp"

#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/sparse_set.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
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

/// An adapter: the library's journal under the names a growable sequence must offer. It only renames.
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

// What it guarantees: it only grows, and entries keep their address.
template<>
struct urd::container_traits<Journal> : urd::properties_of<urd::positional, urd::stable_refs, urd::growable>
{
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
struct urd::container_traits<ClaimsStableRefs> : urd::properties_of<urd::stable_refs, urd::growable>
{
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
struct urd::container_traits<ClaimsStableValues> : urd::properties_of<urd::sorted, urd::unique, urd::dense, urd::stable_refs, urd::shrinkable, urd::retains>
{
};

TEST(Properties, AFalseClaimAboutValuesIsCaught)
{
    const auto found = urd::verify(ClaimsStableValues(), 31).value_or("");
    EXPECT_NE(found.find("stable_refs"), std::string::npos) << found;
}
