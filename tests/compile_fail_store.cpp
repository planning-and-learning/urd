// Store requests and uses that must not compile, checked like compile_fail.cpp: without URD_CASE the file
// compiles; with URD_CASE=<n>, case n alone must fail.

#include "urd/store.hpp"
// Every family: the cases below hold a Store of every kind.
#include "urd/repr/addressable_heap.hpp"
#include "urd/repr/arena.hpp"
#include "urd/repr/bidirectional_csr.hpp"
#include "urd/repr/bucket_queue.hpp"
#include "urd/repr/change_logged.hpp"
#include "urd/repr/csr_multimap.hpp"
#include "urd/repr/dense_multimap.hpp"
#include "urd/repr/dense_set.hpp"
#include "urd/repr/disjoint_sets.hpp"
#include "urd/repr/entry_ordered.hpp"
#include "urd/repr/flat_map.hpp"
#include "urd/repr/flat_set.hpp"
#include "urd/repr/index_map.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/repr/jagged.hpp"
#include "urd/repr/layer.hpp"
#include "urd/repr/layered_pool.hpp"
#include "urd/repr/ordered_map.hpp"
#include "urd/repr/parallel_hash.hpp"
#include "urd/repr/pools.hpp"
#include "urd/repr/postings.hpp"
#include "urd/repr/queue.hpp"
#include "urd/repr/records.hpp"
#include "urd/repr/recycling.hpp"
#include "urd/repr/ring_fifo.hpp"
#include "urd/repr/row_pool.hpp"
#include "urd/repr/scoped.hpp"
#include "urd/repr/segmented.hpp"
#include "urd/repr/sequence.hpp"
#include "urd/repr/slots.hpp"
#include "urd/repr/sparse_set.hpp"
#include "urd/repr/std_map.hpp"
#include "urd/repr/std_set.hpp"
#include "urd/repr/tree_bitset.hpp"
#include "urd/repr/tree_vector.hpp"
#include "urd/repr/unordered_flat_map.hpp"
#include "urd/repr/unordered_flat_set.hpp"
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
#elif URD_CASE == 4  // functional is derived (a map without multi), never stated
    using S = Store<key<int>, value<int>, urd::functional>;
    (void) S();
#elif URD_CASE == 5
    using S = Store<key<int>, urd::dense, urd::sparse>;
    (void) S();
#elif URD_CASE == 6   // contiguous storage moves elements when it grows
    using S = Store<key<int>, urd::contiguous, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 7   // dense keys need a representation for them; strings have none
    using S = Store<key<std::string>, urd::dense>;
    (void) S();
#elif URD_CASE == 8   // one key type only
    using S = Store<key<int>, key<long>>;
    (void) S();
#elif URD_CASE == 9  // only urd properties
    using S = Store<key<int>, int>;
    (void) S();
#elif URD_CASE == 10  // changing an element in place could break the order
    sorted[0] = 3;
#elif URD_CASE == 11  // a nonempty Store cannot start empty
    (void) Store<key<int>, urd::nonempty>();
#elif URD_CASE == 12  // erasing from a sequence moves later elements: no representation keeps references then
    using S = Store<value<int>, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 13  // memory is either kept for reuse or returned
    using S = Store<key<int>, urd::retains, urd::reclaims>;
    (void) S();
#elif URD_CASE == 14  // a frozen row is a view: it never changes
    auto rows = Store<value<Store<value<int>, urd::frozen>>> { { 1, 2 } };
    rows[0].push_back(3);
#elif URD_CASE == 15  // a frozen map gains no keys
    auto constants = Store<key<int>, value<int>, urd::frozen> { { 1, 2 } };
    constants[3] = 4;
#elif URD_CASE == 16  // a multimap has many values per key, so none to emplace against
    auto many = Store<key<int>, value<int>, urd::multi>();
    many.try_emplace(1, 2);
#elif URD_CASE == 17  // a key is never written through an iterator: it would break the order
    auto sorted_map = Store<key<int>, value<int>, urd::sorted> { { 1, 2 } };
    sorted_map.begin()->first = 3;
#elif URD_CASE == 18  // a frozen map's values never change
    auto constants = Store<key<int>, value<int>, urd::frozen> { { 1, 2 } };
    constants.insert_or_assign(1, 4);
#elif URD_CASE == 19  // a key with no identity, order or hash: no container can keep it
    struct Opaque
    {
        int value;
    };
    (void) Store<key<Opaque>>();
#elif URD_CASE == 20  // an interner's element is its index's: changing it in place would break the lookup
    auto symbols = Store<key<int>, urd::positional> { 1, 2 };
    symbols[0] = 3;
#elif URD_CASE == 21  // a queue is iterated but compares by no ==: a heap's order is no content
    auto open = Store<value<int>, urd::by_priority>();
    (void) (open == open);
#elif URD_CASE == 22  // a queue offers the next element by its order's name: top for a priority queue
    auto open = Store<value<int>, urd::by_priority>();
    (void) open.front();
#elif URD_CASE == 23  // a pool is reached by handle, never iterated
    auto states = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique>();
    (void) states.begin();
#elif URD_CASE == 24  // a pool's rows are values; a key would identify them otherwise
    using S = Store<key<int>, urd::handles>;
    (void) S();
#elif URD_CASE == 26  // a map ordered by its values hands out no value to change: writing through it would break the order
    using S = Store<key<int>, value<int>, urd::sorted, urd::small, urd::order_by<[](int, int value) { return value; }>>;
    auto ordered = S();
    ordered.insert_or_assign(1, 2);
    ordered.at(1) = 3;
#elif URD_CASE == 27  // a set has no values to order by
    using S = Store<key<int>, urd::sorted, urd::order_by<[](int key, int value) { return key + value; }>>;
    (void) S();
#elif URD_CASE == 28  // an unordered Store iterates in no order its contents fix, so it is no hashed key
    using S = Store<key<Store<key<int>>>>;
    (void) S();
#elif URD_CASE == 29  // dense alone does not say sorted: the key must ask for the order its hash follows
    using S = Store<key<Store<key<int>, urd::dense, urd::frozen>>>;
    (void) S();
#elif URD_CASE == 30  // a view needs a context that resolves its index: a hash set has no indices
    using Items = Store<key<int>>;
    [[maybe_unused]] auto items = Items { 1 };
    [[maybe_unused]] auto item = urd::view<int, Items>(urd::index<int>(0), items);
#elif URD_CASE == 31  // an index of A is no index of B
    struct A;
    struct B;
    auto by_a = Store<key<urd::index<A>>, value<int>, urd::dense>();
    (void) by_a.contains(urd::index<B>(0));
#elif URD_CASE == 32  // a pool of records (no range of elements) stores them flat: it must say so
    struct Point
    {
        int x = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };
    using S = Store<value<Point>, urd::handles, urd::unique>;
    (void) S();
#elif URD_CASE == 33  // flat is how an interning pool stores records, not a property of a set
    using S = Store<key<int>, urd::flat>;
    (void) S();
#elif URD_CASE == 34  // a record with a std::string is neither trivially copyable nor serializable: nothing stores it flat
    struct Note
    {
        std::string text;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Note::text> {}; }
    };
    using S = Store<value<Note>, urd::handles, urd::unique, urd::flat>;
    (void) S();
#elif URD_CASE == 35  // a stored record is frozen: read by index, never written through it
    struct Point
    {
        int x = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Point::x> {}; }
    };
    auto points = Store<value<Point>, urd::handles, urd::unique, urd::flat>();
    const auto at = points.insert(Point { 1 }).first;
    points[at].x = 2;
#elif URD_CASE == 36  // a pool of slots has no layers: nothing continues its handles over a parent
    using S = Store<value<int>, urd::handles, urd::layered>;
    (void) S();
#elif URD_CASE == 37  // a tree pool numbers nodes, not rows: no layer continues its handles
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::layered>;
    (void) S();
#elif URD_CASE == 38  // a layered dense map shared by threads has no representation yet (the concurrent slot directory)
    struct Predicate;
    using Rows = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::monotone, urd::layered, urd::concurrent>;
    using S = Store<key<urd::index<Predicate>>, value<Rows>, urd::dense, urd::layered, urd::concurrent>;
    (void) S();
#elif URD_CASE == 39  // a partition groups keys: no values, and it changes by merge
    using S = Store<key<int>, value<int>, urd::partition>;
    (void) S();
#elif URD_CASE == 40  // fixed_shape is a sequence of rows, each a Store
    using S = Store<value<int>, urd::fixed_shape>;
    (void) S();
#elif URD_CASE == 41  // no concurrent sorted container: threads share a hashed or a dense Store
    using S = Store<key<int>, value<int>, urd::sorted, urd::concurrent>;
    (void) S();
#elif URD_CASE == 42  // a queue is reached by its front or top, which pop moves: no queue is concurrent
    using S = Store<value<int>, urd::fifo, urd::concurrent>;
    (void) S();
#elif URD_CASE == 43  // inserting at a position moves the elements after it: a positional set with stable_refs needs monotone
    using S = Store<key<int>, urd::positional, urd::stable_refs>;
    (void) S();
#elif URD_CASE == 44  // dense keys are stored by position: erase returns no storage
    using S = Store<key<int>, urd::dense, urd::reclaims>;
    (void) S();
#elif URD_CASE == 45  // constant_clear is a generation counter over a slot per index: it needs dense keys
    using S = Store<key<int>, urd::constant_clear>;
    (void) S();
#elif URD_CASE == 46  // dense keys are iterated by index: no order_by applies
    using S = Store<key<int>, urd::dense, urd::sorted, urd::order_by<[](int key) { return -key; }>>;
    (void) S();
#elif URD_CASE == 47  // a layer iterates its own elements, not its parent's: no order holds over both
    using S = Store<key<int>, urd::layered, urd::sorted>;
    (void) S();
#elif URD_CASE == 48  // a partition's merge rewrites parents along two paths: single-threaded
    using S = Store<key<int>, urd::partition, urd::concurrent>;
    (void) S();
#elif URD_CASE == 49  // a layered Store's contents include its parent's: no ==
    using S = Store<key<int>, urd::layered>;
    (void) (S() == S());
#elif URD_CASE == 50  // erase moves the last row in: not where the pool only grows
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::monotone>;
    (void) S().erase({});
#elif URD_CASE == 51  // ... nor where handles name shared nodes
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>;
    (void) S().erase({});
#elif URD_CASE == 52  // ... nor where threads read without a lock
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::concurrent>;
    (void) S().erase({});
#elif URD_CASE == 53  // ... nor where layers continue the handles
    using S = Store<value<Store<value<int>, urd::frozen>>, urd::handles, urd::unique, urd::layered>;
    (void) S().erase({});
#elif URD_CASE == 54  // dense keys have no O(1) least element: their first is found by a scan
    using S = Store<key<int>, urd::dense, urd::sorted>;
    (void) S().front();
#elif URD_CASE == 55  // a bucket queue would visit every priority's bucket
    using S = Store<value<int>, urd::by_priority, urd::monotone_priority>;
    (void) S().begin();
#elif URD_CASE == 56  // only a fifo queue has a back
    using S = Store<value<int>, urd::lifo>;
    (void) S().back();
#elif URD_CASE == 57  // a queue without handles has nothing to read an element by
    using S = Store<value<int>, urd::by_priority>;
    (void) S()[0];
#elif URD_CASE == 58  // a partition holds each element once
    using S = Store<key<int>, urd::partition>;
    (void) S().count(0);
#elif URD_CASE == 59  // a layered pool of records reaches each through the layer that owns it
    struct Name
    {
        int id = 0;
        static constexpr auto identifying_members() noexcept { return urd::Members<&Name::id> {}; }
    };
    using S = Store<value<Name>, urd::handles, urd::unique, urd::flat, urd::layered>;
    (void) S().begin();
#elif URD_CASE == 25  // unique keys ordered by a projection need their own order to break ties
    struct Unordered
    {
        int cost;
    };
    using S = Store<key<Unordered>, urd::sorted, urd::order_by<&Unordered::cost>>;
    (void) S();
#endif
}
