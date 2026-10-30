// Store requests and uses that must not compile, checked like compile_fail.cpp: without URD_CASE the file
// compiles; with URD_CASE=<n>, case n alone must fail.

#include "urd/compose/family.hpp"
#include "urd/store.hpp"
// Every family: the cases below hold a Store of every kind.
#include "urd/use/contiguous.hpp"
#include "urd/use/dense.hpp"
#include "urd/use/hash.hpp"
#include "urd/use/layers.hpp"
#include "urd/use/nodes.hpp"
#include "urd/use/pools.hpp"
#include "urd/use/queues.hpp"
#include "urd/use/records.hpp"
#include "urd/use/scopes.hpp"
#include "urd/use/sequences.hpp"
#if defined(URD_HAS_GTL)
#include "urd/use/parallel.hpp"
#endif
#include "urd/view.hpp"

#include <string>

using urd::key;
using urd::Store;
using urd::value;

template<typename T>
using SetOf = Store<key<T>>;

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
#elif URD_CASE == 25  // a family reaches only the types it was given
    (void) urd::compose::Family<SetOf, int, long>().get<short>();
#elif URD_CASE == 26  // a pool is reached by handle, never iterated
    auto states = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>();
    (void) states.begin();
#elif URD_CASE == 27  // a pool's rows are values; a key would identify them otherwise
    using S = Store<key<int>, urd::handles>;
    (void) S();
#elif URD_CASE == 28  // order_by orders a sorted Store or a priority queue; an unordered set has no order to change
    using S = Store<key<std::pair<int, int>>, urd::order_by<&std::pair<int, int>::first>>;
    (void) S();
#elif URD_CASE == 30  // a map ordered by its values hands out no value to change: writing through it would break the order
    using S = Store<key<int>, value<int>, urd::sorted, urd::small, urd::order_by<[](int, int value) { return value; }>>;
    auto ordered = S();
    ordered.insert_or_assign(1, 2);
    ordered.at(1) = 3;
#elif URD_CASE == 31  // a set has no values to order by
    using S = Store<key<int>, urd::sorted, urd::order_by<[](int key, int value) { return key + value; }>>;
    (void) S();
#elif URD_CASE == 32  // an unordered Store iterates in no order its contents fix, so it is no hashed key
    using S = Store<key<Store<key<int>>>>;
    (void) S();
#elif URD_CASE == 33  // dense alone does not say sorted: the key must ask for the order its hash follows
    using S = Store<key<Store<key<int>, urd::dense, urd::frozen>>>;
    (void) S();
#elif URD_CASE == 34  // a view needs a context that resolves its index: a hash set has no indices
    using Items = Store<key<int>>;
    [[maybe_unused]] auto items = Items { 1 };
    [[maybe_unused]] auto item = urd::view<int, Items>(urd::index<int>(0), items);
#elif URD_CASE == 35  // an index of A is no index of B
    struct A;
    struct B;
    auto by_a = Store<key<urd::index<A>>, value<int>, urd::dense>();
    (void) by_a.contains(urd::index<B>(0));
#elif URD_CASE == 36  // a pool of records (no range of elements) stores them flat: it must say so
    struct Point
    {
        int x = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };
    using S = Store<value<Point>, urd::handles, urd::unique>;
    (void) S();
#elif URD_CASE == 37  // flat is how an interning pool stores records, not a property of a set
    using S = Store<key<int>, urd::flat>;
    (void) S();
#elif URD_CASE == 38  // a record with a std::string is neither trivially copyable nor serializable: nothing stores it flat
    struct Note
    {
        std::string text;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    };
    using S = Store<value<Note>, urd::handles, urd::unique, urd::flat>;
    (void) S();
#elif URD_CASE == 39  // a stored record is frozen: read by index, never written through it
    struct Point
    {
        int x = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };
    auto points = Store<value<Point>, urd::handles, urd::unique, urd::flat>();
    const auto at = points.insert(Point { 1 }).first;
    points[at].x = 2;
#elif URD_CASE == 40  // a pool of slots has no layers: nothing continues its handles over a parent
    using S = Store<value<int>, urd::handles, urd::layered>;
    (void) S();
#elif URD_CASE == 41  // a tree pool numbers nodes, not rows: no layer continues its handles
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::layered>;
    (void) S();
#elif URD_CASE == 42  // a family resolves the indices of its members' types only
    template<typename T>
    using Interned = Store<key<T>, urd::positional, urd::monotone>;
    struct Unlisted;
    (void) urd::compose::Family<Interned, int, long>().view(urd::index<Unlisted>(0));
#elif URD_CASE == 43  // a layered dense map shared by threads has no representation yet (the concurrent slot directory)
    struct Predicate;
    using Rows = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::monotone, urd::layered, urd::concurrent>;
    using S = Store<key<urd::index<Predicate>>, value<Rows>, urd::dense, urd::layered, urd::concurrent>;
    (void) S();
#elif URD_CASE == 44  // nonempty is a promise about iterated elements: a queue empties by pop
    using S = Store<value<int>, urd::fifo, urd::nonempty>;
    (void) S();
#elif URD_CASE == 45  // a partition groups keys: no values, and it changes by merge
    using S = Store<key<int>, value<int>, urd::partition>;
    (void) S();
#elif URD_CASE == 46  // monotone_priority says how a priority queue is pushed to: it needs the queue
    using S = Store<value<int>, urd::monotone_priority>;
    (void) S();
#elif URD_CASE == 47  // fixed_shape is a sequence of rows, each a Store
    using S = Store<value<int>, urd::fixed_shape>;
    (void) S();
#elif URD_CASE == 48  // no concurrent sorted container: threads share a hashed or a dense Store
    using S = Store<key<int>, value<int>, urd::sorted, urd::concurrent>;
    (void) S();
#elif URD_CASE == 49  // a queue is reached by its front or top, which pop moves: no queue is concurrent
    using S = Store<value<int>, urd::fifo, urd::concurrent>;
    (void) S();
#elif URD_CASE == 50  // inserting at a position moves the elements after it: a positional set with stable_refs needs monotone
    using S = Store<key<int>, urd::positional, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 51  // a frozen Store is read by any number of threads: concurrent asks for inserts it has none of
    using S = Store<key<int>, urd::frozen, urd::concurrent>;
    (void) S();
#elif URD_CASE == 52  // dense keys are stored by position: erase returns no storage
    using S = Store<key<int>, urd::dense, urd::reclaims>;
    (void) S();
#elif URD_CASE == 53  // constant_clear is a generation counter over a slot per index: it needs dense keys
    using S = Store<key<int>, urd::constant_clear>;
    (void) S();
#elif URD_CASE == 54  // dense keys are iterated by index: no order_by applies
    using S = Store<key<int>, urd::dense, urd::sorted, urd::order_by<[](int key) { return -key; }>>;
    (void) S();
#elif URD_CASE == 55  // a map ordered by its values is a flat scan: it is for few or frozen entries
    using S = Store<key<int>, value<int>, urd::sorted, urd::order_by<[](int, int value) { return value; }>>;
    (void) S();
#elif URD_CASE == 56  // a layer iterates its own elements, not its parent's: no order holds over both
    using S = Store<key<int>, urd::layered, urd::sorted>;
    (void) S();
#elif URD_CASE == 57  // a partition's merge rewrites parents along two paths: single-threaded
    using S = Store<key<int>, urd::partition, urd::concurrent>;
    (void) S();
#elif URD_CASE == 58  // a layered Store's contents include its parent's: no ==
    using S = Store<key<int>, urd::layered>;
    (void) (S() == S());
#elif URD_CASE == 29  // unique keys ordered by a projection need their own order to break ties
    struct Unordered
    {
        int cost;
    };
    using S = Store<key<Unordered>, urd::sorted, urd::order_by<&Unordered::cost>>;
    (void) S();
#endif
}
