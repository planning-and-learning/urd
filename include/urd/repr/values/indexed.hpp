#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bit_rows.hpp"
#include "urd/repr/containers/flat_rows.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/core/boost_memory.hpp"
#include "urd/repr/core/gtl_memory.hpp"
#include "urd/repr/core/gtl_spin_lock.hpp"
#include "urd/repr/values/indexed_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

namespace detail
{

/// What a set of rows (IndexedSet, Rows) keeps of T, a range of plain elements (a vector, a sequence Store): the
/// elements, and the span a row is stored, handed out and looked up as. Elsewhere T itself.
template<typename T, bool Rows>
struct row_key
{
    using element = void;
    using type = T;
};
template<typename T>
struct row_key<T, true>
{
    using element = std::ranges::range_value_t<T>;
    using type = std::span<const element>;
};

/// A row a set of rows takes besides its elements: a contiguous range of the rows' elements (a span, a vector).
template<typename R, typename U>
concept row_of = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> && std::same_as<std::ranges::range_value_t<R>, U>;

template<typename X>
struct prehashed_key
{
    using type = void;
};
template<typename T>
struct prehashed_key<prehashed<T>>
{
    using type = T;
};
/// A row, or a row with its hash (prehashed<R>): what a set of rows looks up and inserts by besides its elements.
template<typename X, typename U>
concept row_input = row_of<X, U> || (is_prehashed<X> && row_of<typename prehashed_key<X>::type, U>);

}  // namespace detail

/// A set kept in insertion order: the elements in a vector, and a hash table of their positions that hashes and
/// compares through the vector, so each element is stored once. An element's position is its index, which never
/// changes while nothing is erased: an interner. The vector lives on the heap, where the table's hash and equality
/// find it however the set moves. The registry names it for any element type, so it is an unconstrained template;
/// its container_traits declare properties only for hashable elements.
///
/// With Stable, the elements lie in a SegmentedVector, whose elements never move, so a reference to one holds while
/// others are inserted; it has no erase (positions would shift) and no data(). With ThreadSafe (which needs Stable),
/// threads may insert at once, and read while others insert (an interner shared across threads, the node table of a
/// thread-safe tree pool): the segments are the thread-safe ones and a concurrent hash table holds the positions.
/// Looking up is lock-free; an insert of a new element takes a lock, so positions stay 0..n-1 without gaps. Clear,
/// copy and move need no other operation running.
///
/// With Rows, T is a sequence of plain elements (a row), and the rows are stored flat (urd/repr/containers/flat_rows.hpp:
/// while all have one width, no offsets), handed out as spans of their elements, and looked up and inserted by any
/// contiguous range of them: no row is built to look one up. Rows never move on insert; erase rebuilds.
///
/// With Bits (and Rows), the rows' elements are indices stored in a bit width given at construction
/// (urd/repr/containers/bit_rows.hpp), handed out as decoding ranges.
template<typename T, typename Index, bool ThreadSafe, bool Stable, bool Rows, bool Bits>
class IndexedSet
{
    static_assert(!Bits || Rows, "urd::repr::IndexedSet: bits are a storage of rows");
    static_assert(!ThreadSafe || Stable, "urd::repr::IndexedSet: threads share elements that never move (Stable)");
    static_assert(!Rows || Stable, "urd::repr::IndexedSet: rows lie in segments that never move (Stable)");
    friend struct urd::memory_of<IndexedSet>;
    using row_element = typename detail::row_key<T, Rows>::element;
    /// What the table hashes and compares: the element, or a row's span.
    using Key = typename detail::row_key<T, Rows>::type;
    using Elements = std::conditional_t<Rows,
                                        std::conditional_t<Bits, BitRows<row_element, ThreadSafe>, FlatRows<row_element, ThreadSafe>>,
                                        std::conditional_t<Stable, SegmentedVector<T, default_first_segment_size, ThreadSafe>, std::vector<T>>>;

    /// A row (a span, or a stored row however its storage hands it out) hashes as its elements: alike either way.
    template<typename K>
    [[nodiscard]] static hash_t hash_key(const K& key)
    {
        if constexpr (Rows)
            return hash_range(key);
        else
            return Hash<T> {}(key);
    }
    template<typename L, typename R>
    [[nodiscard]] static bool equal_keys(const L& lhs, const R& rhs)
    {
        if constexpr (Rows)
            return equal_range(lhs, rhs);
        else
            return EqualTo<T> {}(lhs, rhs);
    }
    /// What the table looks `x` up by: a row as the span of its elements, else the element itself.
    template<typename X>
    [[nodiscard]] static decltype(auto) key_of(const X& x)
    {
        if constexpr (Rows)
            return Key(std::ranges::data(x), std::ranges::size(x));
        else
            return (x);
    }

    /// A position in the vector, as the table holds it: a type of its own, so that it is never taken for an element,
    /// even where the elements are integers too.
    struct Position
    {
        Index value;

        friend bool operator==(Position, Position) = default;
    };
    /// A position about to enter the table, with its element's hash: inserted as itself (it becomes a Position), so
    /// that the table does not hash the element again.
    struct Placed
    {
        Index value;
        hash_t hash;

        operator Position() const noexcept { return Position { value }; }  // NOLINT(google-explicit-constructor)
    };

    /// Hashes a position by its element, and an element as itself, so the table can be searched by element.
    struct PositionHash
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] std::size_t operator()(Position position) const { return hash_key((*elements)[position.value]); }
        [[nodiscard]] std::size_t operator()(const Key& element) const { return hash_key(element); }
        [[nodiscard]] std::size_t operator()(const prehashed<Key>& key) const noexcept { return key.hash; }
        [[nodiscard]] std::size_t operator()(Placed placed) const noexcept { return placed.hash; }
    };
    struct PositionEqual
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] bool operator()(Position lhs, Position rhs) const { return equal_keys((*elements)[lhs.value], (*elements)[rhs.value]); }
        [[nodiscard]] bool operator()(const Key& lhs, Position rhs) const { return equal_keys(lhs, (*elements)[rhs.value]); }
        [[nodiscard]] bool operator()(Position lhs, const Key& rhs) const { return equal_keys((*elements)[lhs.value], rhs); }
        [[nodiscard]] bool operator()(const Key& lhs, const Key& rhs) const { return equal_keys(lhs, rhs); }
        [[nodiscard]] bool operator()(const prehashed<Key>& lhs, Position rhs) const { return (*this)(lhs.key, rhs); }
        [[nodiscard]] bool operator()(Position lhs, const prehashed<Key>& rhs) const { return (*this)(lhs, rhs.key); }
        [[nodiscard]] bool operator()(Placed lhs, Position rhs) const { return (*this)(Position { lhs.value }, rhs); }
        [[nodiscard]] bool operator()(Position lhs, Placed rhs) const { return (*this)(lhs, Position { rhs.value }); }
    };
    /// Threads share gtl's sharded table, its 2^7 shards each under a shared spin lock (yggdrasil's interners
    /// benchmarked this against boost's concurrent table and std::shared_mutex: faster lookups and inserts).
    /// A set of rows that erases (a relation's rows) keeps gtl's table, which cleans erased slots in place where it has
    /// room: Boost's rehashes after erasures, allocating.
    using Table = std::conditional_t<
        ThreadSafe,
        gtl::parallel_flat_hash_set<Position, PositionHash, PositionEqual, std::allocator<Position>, 7, SpinSharedMutex>,
        std::conditional_t<Rows, gtl::flat_hash_set<Position, PositionHash, PositionEqual>, boost::unordered_flat_set<Position, PositionHash, PositionEqual>>>;

public:
    using iterator = typename Elements::const_iterator;
    using const_iterator = iterator;
    using value_type = T;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndexedSet()
        requires(!Bits)
    = default;
    /// Rows whose elements each fit in `width` (a pool of urd::bit).
    explicit IndexedSet(urd::bit_width width)
        requires Bits
        : m_width(width)
    {
    }

    template<std::input_iterator InputIt>
        requires(!Bits)
    IndexedSet(InputIt first, InputIt last)
    { insert(first, last); }

    IndexedSet(const IndexedSet& other) : m_width(other.m_width) { insert(other.begin(), other.end()); }
    IndexedSet& operator=(const IndexedSet& other)
    {
        if (this != &other)
            *this = IndexedSet(other);
        return *this;
    }
    /// The moved-from set keeps a table and elements of its own, empty.
    IndexedSet(IndexedSet&& other) noexcept :
        m_width(other.m_width),
        m_elements(std::exchange(other.m_elements, other.fresh_elements())),
        m_positions(std::exchange(other.m_positions, table_for(other.elements())))
    {
    }
    IndexedSet& operator=(IndexedSet&& other) noexcept
    {
        if (this != &other)
        {
            m_width = other.m_width;
            m_elements = std::exchange(other.m_elements, other.fresh_elements());
            m_positions = std::exchange(other.m_positions, table_for(other.elements()));
        }
        return *this;
    }
    ~IndexedSet() = default;

    [[nodiscard]] iterator begin() const { return elements().cbegin(); }
    [[nodiscard]] iterator end() const { return elements().cend(); }
    [[nodiscard]] size_type size() const { return elements().size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    /// The bit width of the rows' elements (Bits).
    [[nodiscard]] urd::bit_width width() const noexcept
        requires Bits
    { return m_width; }
    /// The element, or a row's span.
    [[nodiscard]] decltype(auto) operator[](size_type position) const { return elements()[position]; }
    [[nodiscard]] const T* data() const
        requires(!Stable)
    { return elements().data(); }

    void reserve(size_type count)
        requires(!Stable)
    {
        writable().reserve(count);
        m_positions.reserve(count);
    }
    // Every lookup and insert takes the element itself or a prehashed<T> (urd/elements/prehashed.hpp), which is not
    // hashed again; either way the element is hashed once, an insert included.
    [[nodiscard]] iterator find(const T& element) const { return found_at(lookup(element)); }
    [[nodiscard]] iterator find(const prehashed<T>& key) const { return found_at(lookup(key)); }
    /// A set of rows looks a row up by any contiguous range of its elements.
    template<detail::row_input<row_element> R>
        requires(Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>>)
    [[nodiscard]] iterator find(const R& row) const
    { return found_at(lookup(row)); }
    /// The position of `element`, or none. The lookup for a set threads insert into: an iterator from find compared
    /// with end() races with the size, which another thread's insert moves between the two calls.
    [[nodiscard]] std::optional<size_type> position(const T& element) const { return position_from(lookup(element)); }
    [[nodiscard]] std::optional<size_type> position(const prehashed<T>& key) const { return position_from(lookup(key)); }
    template<detail::row_input<row_element> R>
        requires(Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>>)
    [[nodiscard]] std::optional<size_type> position(const R& row) const
    { return position_from(lookup(row)); }
    [[nodiscard]] size_type count(const T& element) const { return contains(element) ? 1 : 0; }
    [[nodiscard]] size_type count(const prehashed<T>& key) const { return contains(key) ? 1 : 0; }
    template<detail::row_input<row_element> R>
        requires(Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>>)
    [[nodiscard]] size_type count(const R& row) const
    { return contains(row) ? 1 : 0; }
    [[nodiscard]] bool contains(const T& element) const { return lookup(element).has_value(); }
    [[nodiscard]] bool contains(const prehashed<T>& key) const { return lookup(key).has_value(); }
    template<detail::row_input<row_element> R>
        requires(Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>>)
    [[nodiscard]] bool contains(const R& row) const
    { return lookup(row).has_value(); }

    /// The element's position, appending it where it is new. Thread-safe, a lookup first, without a lock; a new
    /// element is appended and indexed under the lock, looked up again there in case another thread inserted it.
    std::pair<iterator, bool> insert(const T& element) { return inserted(element, hash_key(key_of(element))); }
    std::pair<iterator, bool> insert(T&& element)
    {
        const auto hash = hash_key(key_of(element));
        return inserted(std::move(element), hash);
    }
    /// A copy of the key where it is new.
    std::pair<iterator, bool> insert(const prehashed<T>& key) { return inserted(key.key, key.hash); }
    /// A set of rows inserts a row from any contiguous range of its elements, or one with its hash (urd::hash_range
    /// of the elements), copied where it is new.
    template<detail::row_input<row_element> R>
        requires(Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>>)
    std::pair<iterator, bool> insert(const R& row)
    {
        if constexpr (is_prehashed<R>)
            return inserted(row.key, row.hash);
        else
            return inserted(row, hash_key(key_of(row)));
    }
    /// New elements go last, wherever the hint is.
    iterator insert(const_iterator /*hint*/, const T& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last,
                      [this](const auto& element)
                      {
                          if constexpr (Rows && detail::row_of<std::remove_cvref_t<decltype(element)>, row_element>)
                              this->insert(element);
                          else if constexpr (Rows)  // a stored row handed out as a range (bits): its elements
                              this->insert(T(std::ranges::begin(element), std::ranges::end(element)));
                          else
                              this->insert(T(element));
                      });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }
    /// The position of `key`, appending what `make(position)` gives where it is new: the factory runs under the
    /// writer lock with the position reserved, for an element that holds its own position, and must give an element
    /// equal to `key`. It must not touch this set.
    template<std::invocable<std::size_t> Factory>
        requires(!Rows)
    std::pair<iterator, bool> insert_with(const T& key, Factory make)
    { return insert_with(prehash(key), std::move(make)); }
    template<std::invocable<std::size_t> Factory>
        requires(!Rows)
    std::pair<iterator, bool> insert_with(const prehashed<T>& key, Factory make)
    {
        // Threads: one search, under the shard's lock (intern_concurrently); else a search, then the append.
        if constexpr (ThreadSafe)
            return intern_concurrently(key, [&](Index position) -> decltype(auto) { return make(static_cast<std::size_t>(position)); });
        if (const auto found = position_of(key))
            return { at(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        const auto position = static_cast<Index>(size());
        writable().push_back(make(static_cast<std::size_t>(position)));
        assert(EqualTo<T> {}(elements()[position], key.key) && "urd::repr::IndexedSet::insert_with: the factory must give an element equal to the key");
        place(position, key);
        return { at(position), true };
    }

    size_type erase(const T& element)
        requires(!Stable || (Rows && !ThreadSafe))
    {
        const auto found = lookup(element);
        if (!found)
            return 0;
        erase(at(*found));
        return 1;
    }
    iterator erase(const_iterator pos)
        requires(!Stable || (Rows && !ThreadSafe))
    { return erase(pos, std::next(pos)); }
    /// Later elements move up to keep the order, so their positions change.
    // ponytail: O(n) per erase, reindexing what follows; an interner never erases.
    iterator erase(const_iterator first, const_iterator last)
        requires(!Stable || (Rows && !ThreadSafe))
    {
        const auto from = static_cast<Index>(first - begin());
        if constexpr (Rows)
        {
            // Rows never move, so the rest are copied into new storage and indexed again: every row's span changes.
            const auto to = static_cast<Index>(last - begin());
            auto kept = make_elements();
            for (Index position = 0; position < size(); ++position)
                if (position < from || position >= to)
                    kept->push_back(elements()[position]);
            m_elements = std::move(kept);
            m_positions = table_for(*m_elements);
            for (Index position = 0; position < size(); ++position)
                m_positions.insert(Position { position });
            return at(from);
        }
        else
        {
            for (auto position = from; position < size(); ++position)
                m_positions.erase(Position { position });
            writable().erase(first, last);
            for (auto position = from; position < size(); ++position)
                m_positions.insert(Position { position });
            return at(from);
        }
    }
    /// Erases the element at `position` by moving the last one into its place: O(1) expected, and only the last
    /// element's position changes (to `position`). The order is not kept.
    void swap_erase(Index position)
        requires(!Stable || (Rows && !ThreadSafe))
    {
        assert(position < size());
        const auto last = static_cast<Index>(size() - 1);
        // Room for twice the elements before erasing, so that gtl's table cleans its erased slots in place, never growing
        // (allocating) as rows come and go.
        if constexpr (Rows)
            m_positions.reserve(size() * 2);
        // The table hashes a position by the element there: each leaves the table before its element moves.
        m_positions.erase(Position { position });
        if (position != last)
            m_positions.erase(Position { last });
        if constexpr (Rows)
            writable().swap_erase(position);
        else
        {
            if (position != last)
                writable()[position] = std::move(writable()[last]);
            writable().pop_back();
        }
        if (position != last)
            m_positions.insert(Position { position });
    }
    void clear()
    {
        m_positions.clear();
        if (m_elements)
            m_elements->clear();
    }

    /// Equal elements in the same order.
    [[nodiscard]] friend bool operator==(const IndexedSet& lhs, const IndexedSet& rhs)
    { return std::ranges::equal(lhs, rhs, [](const auto& a, const auto& b) { return equal_keys(a, b); }); }

private:
    [[nodiscard]] static Table table_for(const Elements& elements) { return Table(0, PositionHash { &elements }, PositionEqual { &elements }); }
    [[nodiscard]] iterator at(Index position) const { return begin() + static_cast<difference_type>(position); }
    [[nodiscard]] iterator found_at(std::optional<Index> found) const { return found ? at(*found) : end(); }
    [[nodiscard]] static std::optional<size_type> position_from(std::optional<Index> found)
    { return found ? std::optional<size_type>(static_cast<size_type>(*found)) : std::nullopt; }
    /// The position of `x`: an element, a prehashed<T> of one (not hashed again), or a row.
    template<typename X>
    [[nodiscard]] std::optional<Index> lookup(const X& x) const
    {
        if constexpr (is_prehashed<X>)
        {
            const auto& key = key_of(x.key);
            return position_of(prehashed<Key> { key, x.hash });
        }
        else
        {
            const auto& key = key_of(x);
            return position_of(prehashed<Key> { key, hash_key(key) });
        }
    }
    [[nodiscard]] std::optional<Index> position_of(const prehashed<Key>& key) const
    {
        if constexpr (ThreadSafe)
        {
            // The shard of the key's hash, under its shared lock, searched with that hash: what gtl's if_contains does,
            // without hashing the key again for the shard and again for the search.
            const auto hash = m_positions.hash(key);
            auto found = std::optional<Index>();
            m_positions.with_submap(m_positions.subidx(hash),
                                    [&](const auto& shard)
                                    {
                                        if (const auto it = shard.find(key, hash); it != shard.end())
                                            found = it->value;
                                    });
            return found;
        }
        else
        {
            const auto found = m_positions.find(key);
            return found == m_positions.end() ? std::nullopt : std::optional<Index>(found->value);
        }
    }
    /// `element`, whose hash is `hash`, appended where it is new; the table takes the position with that hash.
    template<typename U>
    std::pair<iterator, bool> inserted(U&& element, hash_t hash)
    {
        const auto& looked_up = key_of(element);
        if constexpr (Bits)
            Elements::check(m_width, looked_up);  // before the table: a row that does not fit changes nothing
        const auto key = prehashed<Key> { looked_up, hash };
        if constexpr (!ThreadSafe)
            if (const auto found = position_of(key))
                return { at(*found), false };
        if constexpr (ThreadSafe)
            return intern_concurrently(key,
                                       [&](Index /*position*/) -> decltype(auto)
                                       {
                                           if constexpr (Rows)
                                               return looked_up;
                                           else
                                               return std::forward<U>(element);
                                       });
        const auto lock = std::scoped_lock(m_writer);
        const auto position = static_cast<Index>(size());
        if constexpr (Rows)
            writable().push_back(looked_up);
        else
            writable().push_back(std::forward<U>(element));
        place(position, key);  // the table takes the position with the key's hash, which is the stored element's
        return { at(position), true };
    }

    /// Where threads share the set: the key's shard locked exclusively, searched once, and where the key is missing, the
    /// element `make(position)` gives appended and its position put in the shard: one search under the lock, as
    /// yggdrasil's interners did; the shard's readers wait for the append. The segments append under their own lock,
    /// numbering the element as they do (positions stay 0..n-1); rows, which have none, under the writer lock.
    template<typename Make>
    std::pair<iterator, bool> intern_concurrently(const prehashed<Key>& key, Make make)
    {
        auto position = Index {};
        auto inserted = false;
        m_positions.lazy_emplace_l(
            key,
            [&position](const Position& existing) { position = existing.value; },
            [&](const auto& construct)
            {
                if constexpr (Rows)
                {
                    const auto lock = std::scoped_lock(m_writer);
                    position = static_cast<Index>(size());
                    writable().push_back(make(position));
                }
                else
                    position = static_cast<Index>(writable().emplace_back_with_index(
                        [&](std::size_t at) -> decltype(auto) { return make(static_cast<Index>(at)); }, std::numeric_limits<Index>::max() - 1));
                construct(Position { position });
                inserted = true;
            });
        return { at(position), inserted };
    }
    /// Indexes a new position with its element's hash (`key`'s), which the table does not compute again: gtl takes
    /// the hash as its own hash() gives it, mixed, which for a prehashed key reads only the hash it carries.
    void place(Index position, const prehashed<Key>& key)
    {
        if constexpr (ThreadSafe || Rows)
            m_positions.emplace_with_hash(m_positions.hash(key), Position { position });
        else
            m_positions.insert(Placed { position, key.hash });
    }
    /// No element storage until the first insert where single-threaded (an empty set owns no memory): reads see an
    /// empty one shared by all, which an empty table, hashing only keys, never reads through. Made at once where
    /// threads share the set, as lookups without a lock cannot race its making.
    [[nodiscard]] std::unique_ptr<Elements> fresh_elements() const
    {
        if constexpr (ThreadSafe)
            return make_elements();
        else
            return nullptr;
    }
    [[nodiscard]] std::unique_ptr<Elements> make_elements() const
    {
        if constexpr (Bits)
            return std::make_unique<Elements>(m_width);
        else
            return std::make_unique<Elements>();
    }
    [[nodiscard]] static const Elements& no_elements()
    {
        if constexpr (Bits)
        {
            static const Elements empty(urd::bit_width { 1 });
            return empty;
        }
        else
        {
            static const Elements empty;
            return empty;
        }
    }
    [[nodiscard]] const Elements& elements() const { return m_elements ? *m_elements : no_elements(); }
    /// The element storage, made where there is none; a table made over no_elements() is empty, and is made again
    /// over the storage.
    Elements& writable()
    {
        if (!m_elements)
        {
            m_elements = make_elements();
            m_positions = table_for(*m_elements);
        }
        return *m_elements;
    }

    /// The bit width of rows' elements (with Bits); before the elements, which are made with it.
    urd::bit_width m_width { 1 };
    std::unique_ptr<Elements> m_elements = fresh_elements();
    Table m_positions = table_for(elements());
    // A spin lock: the critical section is an append and a table insert, short as yggdrasil's benchmarks of its
    // interners found, where it beat std::mutex.
    [[no_unique_address]] std::conditional_t<ThreadSafe, SpinMutex, NoLock> m_writer;
};

}  // namespace urd::repr

namespace urd
{

/// Its elements (on the heap, with their storage) and the table of their positions.
template<typename T, typename Index, bool ThreadSafe, bool Stable, bool Rows, bool Bits>
struct memory_of<repr::IndexedSet<T, Index, ThreadSafe, Stable, Rows, Bits>>
{
    static std::size_t bytes(const repr::IndexedSet<T, Index, ThreadSafe, Stable, Rows, Bits>& held)
    { return (held.m_elements ? sizeof(*held.m_elements) + memory_usage(*held.m_elements) : 0) + memory_usage(held.m_positions); }
};

template<>
struct include<"urd/repr/values/indexed.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<indexed_set_decl, S>
{
    using type = repr::IndexedSet<typename S::element_type>;
};
template<typename S>
struct representation_of<stable_indexed_set_decl, S>
{
    using type = repr::StableIndexedSet<typename S::element_type>;
};
template<typename S>
struct representation_of<concurrent_indexed_set_decl, S>
{
    using type = repr::ConcurrentIndexedSet<typename S::element_type>;
};

}  // namespace urd::detail
