// A partition of indices and a family of Stores named by type.

#include "urd/repr/maps/bidirectional_csr.hpp"
#include "urd/repr/maps/csr_multimap.hpp"
#include "urd/repr/maps/dense_multimap.hpp"
#include "urd/repr/maps/index_map.hpp"
#include "urd/repr/maps/ordered_map.hpp"
#include "urd/repr/maps/postings.hpp"
#include "urd/repr/maps/unordered_flat_map.hpp"
#include "urd/repr/partitions/disjoint_sets.hpp"
#include "urd/repr/values/dense_set.hpp"
#include "urd/repr/values/indexed.hpp"
#include "urd/repr/values/sparse_set.hpp"
#include "urd/repr/values/unordered_flat_set.hpp"
#include "urd/store.hpp"

#include <exception>
#include <iostream>

using urd::partition;
using urd::Store;

namespace
{

void run()
{
    // Classes of equal terms: merge what is equal, ask which class each term is in.
    auto terms = Store<partition<int>>();
    terms.insert(4);  // terms 0..4, each in a class of its own
    terms.merge(0, 3);
    terms.merge(3, 4);
    std::cout << "classes: " << terms.classes() << ", 0 and 4 equal: " << std::boolalpha << terms.same(0, 4) << '\n';
}

}  // namespace

int main()
{
    try
    {
        run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
