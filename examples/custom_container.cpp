// Preparing a container to represent Stores, the way a graph models Boost.Graph's concepts: declare its
// properties in urd::container_traits, offer the operations they require (urd/interface.hpp), and let
// urd::verify check both. A container with the standard's names offers them as it is; one with its own names,
// such as a library's, gets an adapter: free functions of the standard names in its namespace. Downstream code
// never uses it directly: it asks a Store for properties, and the registry picks a representation.

#include "urd/verify.hpp"

#include <cstddef>
#include <deque>
#include <iostream>

namespace lib
{

/// A library's append-only journal, with its own names. Entries never move once written.
class Journal
{
public:
    Journal() = default;
    Journal(const int* first, const int* last) : m_entries(first, last) {}

    [[nodiscard]] auto begin() const { return m_entries.begin(); }
    [[nodiscard]] auto end() const { return m_entries.end(); }
    [[nodiscard]] std::size_t size() const { return m_entries.size(); }

    const int& append(int entry) { return m_entries.emplace_back(entry); }

private:
    std::deque<int> m_entries;
};

// The adapter: what a growable sequence must offer, in the library's namespace, where urd finds it.
inline void push_back(Journal& journal, int entry) { journal.append(entry); }
inline const int& emplace_back(Journal& journal, int entry) { return journal.append(entry); }

}  // namespace lib

// What it guarantees: it only grows, and entries keep their address.
template<>
struct urd::container_traits<lib::Journal> : urd::properties_of<urd::stable_refs, urd::growable>
{
};

int main()
{
    // The claim is checked, not trusted.
    if (const auto violation = urd::verify(lib::Journal(), 1))
    {
        std::cout << "Journal breaks a property: " << *violation << '\n';
        return 1;
    }
    std::cout << "Journal offers what stable_refs and growable require and keeps them through 2000 random operations\n";
}
