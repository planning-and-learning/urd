// Store requests and uses that must not compile, checked like compile_fail.cpp: without URD_CASE the file
// compiles; with URD_CASE=<n>, case n alone must fail.

#include "urd/store.hpp"

#include <string>

using urd::key;
using urd::Store;
using urd::value;

int main()
{
    [[maybe_unused]] auto set = Store<key<int>>();
    [[maybe_unused]] auto small = Store<key<int>, urd::small> { 1, 2 };
    [[maybe_unused]] auto frozen = Store<key<int>, urd::frozen> { 1, 2 };
    [[maybe_unused]] auto growing = Store<value<int>, urd::monotone>();
    [[maybe_unused]] auto sorted = Store<value<int>, urd::sorted> { 1, 2 };

#if URD_CASE == 1  // a set has no positions
    (void) set[0];
#elif URD_CASE == 2  // a frozen Store never changes
    frozen.insert(3);
#elif URD_CASE == 3  // a monotone Store only grows
    growing.pop_back();
#elif URD_CASE == 4  // one value per key, and many: a contradiction
    using S = Store<key<int>, value<int>, urd::functional, urd::multi>;
    (void) S();
#elif URD_CASE == 5
    using S = Store<key<int>, urd::dense, urd::sparse>;
    (void) S();
#elif URD_CASE == 6   // contiguous storage moves elements when it grows
    using S = Store<key<int>, urd::contiguous, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 7   // positional identifies by position, so it has no key
    using S = Store<key<int>, value<int>, urd::positional>;
    (void) S();
#elif URD_CASE == 8   // dense keys need a representation for them; strings have none
    using S = Store<key<std::string>, urd::dense>;
    (void) S();
#elif URD_CASE == 9   // one key type only
    using S = Store<key<int>, key<long>>;
    (void) S();
#elif URD_CASE == 10  // only urd properties
    using S = Store<key<int>, int>;
    (void) S();
#elif URD_CASE == 11  // changing an element in place could break the order
    sorted[0] = 3;
#elif URD_CASE == 12  // a nonempty Store cannot start empty
    (void) Store<key<int>, urd::nonempty>();
#elif URD_CASE == 13  // erasing from a sequence moves later elements: no representation keeps references then
    using S = Store<value<int>, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 14  // memory is either kept for reuse or returned
    using S = Store<key<int>, urd::retains, urd::reclaims>;
    (void) S();
#elif URD_CASE == 15  // a frozen row is a view: it never changes
    auto rows = Store<value<Store<value<int>, urd::frozen>>> { { 1, 2 } };
    rows[0].push_back(3);
#elif URD_CASE == 16  // a frozen map gains no keys
    auto constants = Store<key<int>, value<int>, urd::frozen> { { 1, 2 } };
    constants[3] = 4;
#elif URD_CASE == 17  // a multimap has many values per key, so none to emplace against
    auto many = Store<key<int>, value<int>, urd::multi>();
    many.try_emplace(1, 2);
#elif URD_CASE == 18  // a key is never written through an iterator: it would break the order
    auto sorted_map = Store<key<int>, value<int>, urd::sorted> { { 1, 2 } };
    sorted_map.begin()->first = 3;
#elif URD_CASE == 19  // a frozen map's values never change
    auto constants = Store<key<int>, value<int>, urd::frozen> { { 1, 2 } };
    constants.insert_or_assign(1, 4);
#elif URD_CASE == 20  // a key with no identity, order or hash: no container can keep it
    struct Opaque
    {
        int value;
    };
    (void) Store<key<Opaque>>();
#elif URD_CASE == 21  // an interner's element is its index's: changing it in place would break the lookup
    auto symbols = Store<key<int>, urd::positional> { 1, 2 };
    symbols[0] = 3;
#elif URD_CASE == 22  // a queue is reached by its front or top, never iterated
    auto open = Store<value<int>, urd::by_priority>();
    (void) open.begin();
#elif URD_CASE == 23  // a queue offers the next element by its order's name: top for a priority queue
    auto open = Store<value<int>, urd::by_priority>();
    (void) open.front();
#elif URD_CASE == 24  // a frozen queue could never pop
    using S = Store<value<int>, urd::fifo, urd::frozen>;
    (void) S();
#endif
}
