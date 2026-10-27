// Your own container gets urd's properties the way a graph gets Boost.Graph's: declare them in
// urd::container_traits, give it the common interface in urd::repr::ops, and let urd::verify check the claim.

#include "urd/concepts.hpp"
#include "urd/verify.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <list>

namespace
{

/// A list kept in ascending order: inserting finds the place, and list nodes never move.
class SortedList
{
public:
    [[nodiscard]] auto begin() const { return m_elements.begin(); }
    [[nodiscard]] auto end() const { return m_elements.end(); }
    [[nodiscard]] std::size_t size() const { return m_elements.size(); }

    void insert(int element) { m_elements.insert(std::ranges::upper_bound(m_elements, element), element); }
    std::size_t erase(int element) { return m_elements.remove(element); }
    void clear() { m_elements.clear(); }

private:
    std::list<int> m_elements;
};

}  // namespace

// What it guarantees, through every operation it offers.
template<>
struct urd::container_traits<SortedList> : urd::properties_of<urd::sorted, urd::stable_refs, urd::shrinkable>
{
};

// The common interface urd::verify (and later a Store) uses.
template<>
struct urd::repr::ops<SortedList>
{
    static bool insert(SortedList& list, int element)
    {
        list.insert(element);
        return true;
    }
    static std::size_t erase(SortedList& list, int element) { return list.erase(element); }
    static void clear(SortedList& list) { list.clear(); }
};

namespace
{

/// Needs ascending input, and says so.
template<typename R>
    requires urd::Sorted<R>
int smallest(const R& range)
{
    return *range.begin();
}

}  // namespace

int main()
{
    // The claim is checked, not trusted.
    if (const auto violation = urd::verify(SortedList(), 1))
    {
        std::cout << "SortedList breaks a property: " << *violation << '\n';
        return 1;
    }
    std::cout << "SortedList keeps sorted and stable_refs through 2000 random operations\n";

    auto list = SortedList();
    list.insert(7);
    list.insert(3);
    list.insert(5);
    std::cout << "smallest: " << smallest(list) << '\n';  // compiles because SortedList declares sorted
}
