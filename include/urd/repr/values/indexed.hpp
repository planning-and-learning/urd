#pragma once

#include "urd/elements/equal_to.hpp"
#include "urd/elements/hash.hpp"
#include "urd/elements/prehashed.hpp"
#include "urd/elements/requirements.hpp"
#include "urd/memory.hpp"
#include "urd/registry/declaration.hpp"
#include "urd/repr/containers/bit_rows.hpp"
#include "urd/repr/containers/detail/threading.hpp"
#include "urd/repr/containers/schema_rows.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/core/gtl_memory.hpp"
#include "urd/repr/core/gtl_spin_lock.hpp"
#include "urd/repr/values/indexed_fwd.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::detail
{
struct indexed_access;
}  // namespace urd::detail

namespace urd::repr
{

namespace detail
{

/// How an indexed set's table is searched: the hash of a key, and whether a stored element equals a key. An element is
/// its own key (element_keys); a row is looked up by any range of its elements, as it is (row_keys).
template<typename T>
struct element_keys
{
    template<typename K>
    [[nodiscard]] static hash_t hash(const K& key)
    { return Hash<T> {}(key); }
    template<typename S, typename K>
    [[nodiscard]] static bool equal(const S& stored, const K& key)
    { return EqualTo<T> {}(stored, key); }
};
struct row_keys
{
    template<typename K>
    [[nodiscard]] static hash_t hash(const K& key)
    { return hash_range(key); }
    template<typename S, typename K>
    [[nodiscard]] static bool equal(const S& stored, const K& key)
    { return equal_elements(stored, key); }
};

/// How an indexed set's table of positions is held and searched. One thread: gtl's table, searched, and where the key is
/// new, appended to.
template<typename Position, typename Hash, typename Equal>
struct single_table
{
    using type = gtl::flat_hash_set<Position, Hash, Equal>;
    using mutex = NoLock;
    using index = decltype(Position::value);

    /// The position `key` is held at, or none.
    template<typename K>
    [[nodiscard]] static std::optional<index> find(const type& positions, const prehashed<K>& key)
    {
        const auto found = positions.find(key);
        return found == positions.end() ? std::nullopt : std::optional<index>(found->value);
    }
    /// The position of `key`, and whether it is new: then `append()` stored its element and gave the position, which
    /// enters the table with the key's hash (gtl takes it as its own hash() gives it: for a prehashed key, the hash it
    /// carries).
    template<typename K, typename Append>
    static std::pair<index, bool> find_or_insert(type& positions, mutex& /*writer*/, const prehashed<K>& key, Append append)
    {
        if (const auto found = find(positions, key))
            return { *found, false };
        const auto position = append();
        positions.emplace_with_hash(positions.hash(key), Position { position });
        return { position, true };
    }
};

/// Threads share the set: gtl's sharded table, its 2^7 shards each under a shared spin lock (yggdrasil's interners
/// benchmarked this against boost's concurrent table and std::shared_mutex: faster lookups and inserts). A lookup takes
/// the key's shard shared. A new key takes it exclusively, searched once, and appends under the writer lock (a spin
/// lock: the section is an append), so positions stay 0..n-1; the shard's readers wait for the append.
template<typename Position, typename Hash, typename Equal>
struct shared_table
{
    static constexpr std::size_t shard_bits = 7;  ///< 2^7 shards
    using type = gtl::parallel_flat_hash_set<Position, Hash, Equal, std::allocator<Position>, shard_bits, SpinSharedMutex>;
    using mutex = SpinMutex;
    using index = decltype(Position::value);

    template<typename K>
    [[nodiscard]] static std::optional<index> find(const type& positions, const prehashed<K>& key)
    {
        // The shard of the key's hash, searched with that hash: what gtl's if_contains does, without hashing the key again
        // for the shard and again for the search.
        const auto hash = positions.hash(key);
        auto found = std::optional<index>();
        positions.with_submap(positions.subidx(hash),
                              [&](const auto& shard)
                              {
                                  if (const auto it = shard.find(key, hash); it != shard.end())
                                      found = it->value;
                              });
        return found;
    }
    template<typename K, typename Append>
    static std::pair<index, bool> find_or_insert(type& positions, mutex& writer, const prehashed<K>& key, Append append)
    {
        // Most inserts find their element: looked up first, under the shard's shared lock only.
        if (const auto found = find(positions, key))
            return { *found, false };
        auto position = index {};
        auto inserted = false;
        positions.lazy_emplace_l(
            key,
            [&position](const Position& existing) { position = existing.value; },
            [&](const auto& construct)
            {
                {
                    const auto lock = std::scoped_lock(writer);
                    position = append();
                }
                construct(Position { position });
                inserted = true;
            });
        return { position, inserted };
    }
};

/// Moves the last element into `position` and drops the last: a storage's own swap_erase, or a vector's.
template<typename Elements>
    requires requires(Elements& elements, std::size_t position) { elements.swap_erase(position); }
void swap_erase(Elements& elements, std::size_t position)
{ elements.swap_erase(position); }
template<typename T, typename Allocator>
void swap_erase(std::vector<T, Allocator>& elements, std::size_t position)
{
    if (position + 1 != elements.size())
        elements[position] = std::move(elements.back());
    elements.pop_back();
}

/// What a row storage is built with (its codec's parameter: a bit width, column widths); nothing (std::monostate) elsewhere.
template<typename Storage>
struct parameter_of
{
    using type = std::monostate;
};
template<typename Storage>
    requires requires { typename Storage::parameter; }
struct parameter_of<Storage>
{
    using type = typename Storage::parameter;
};

/// A row a set of rows takes besides a T: any range of what its stored rows (Stored) hold (a span, a stored row, decoded
/// values).
template<typename R, typename Stored>
concept row_of =
    std::ranges::input_range<R> && std::ranges::input_range<Stored> && std::same_as<std::ranges::range_value_t<R>, std::ranges::range_value_t<Stored>>;

/// A row, or a row with its hash (prehashed<R>): what a set of rows is looked up by besides a T.
template<typename X, typename Stored>
concept row_input = row_of<X, Stored> || (is_prehashed<X> && row_of<std::remove_cvref_t<decltype(std::declval<const X&>().key)>, Stored>);

}  // namespace detail

/// A set kept in insertion order: the elements in a vector, and a hash table of their positions that hashes and
/// compares through the vector, so each element is stored once. An element's position is its index, which never
/// changes while nothing is erased: an interner. The vector and the table live on the heap together, made by the first
/// insert (an empty set owns no memory). The registry names it for any element type, so it is an unconstrained template;
/// its container_traits declare properties only for hashable elements.
///
/// With Stable, the elements lie in a SegmentedVector, whose elements never move, so a reference to one holds while
/// others are inserted; it has no erase (positions would shift) and no data(). With ThreadSafe (which needs Stable),
/// threads may insert at once, and read while others insert (an interner shared across threads, the node table of a
/// thread-safe tree pool): the segments are the thread-safe ones and a concurrent hash table holds the positions.
/// Looking up is lock-free; an insert of a new element takes a lock, so positions stay 0..n-1 without gaps. Clear,
/// copy and move need no other operation running.
///
/// With Rows, T is a sequence of plain elements (a row), and the rows are stored flat (urd/repr/containers/uniform_rows.hpp:
/// while all have one length, no offsets), handed out as their storage reads them (spans, decoded values), and looked up
/// by any range of their elements: no row is built to look one up. Rows never move on insert; erase rebuilds.
///
/// Storage is where the elements lie, where not in the vector or the segments (void): appended to (push_back), read by
/// position, cleared, and never moving what it holds. Rows have one: a UniformRows of a codec, which stores the elements
/// as they are (PlainCodec), indices in a bit width given at construction (BitCodec, urd/repr/containers/bit_rows.hpp,
/// handed out as decoding ranges) or unsigned columns by column widths given at construction (SchemaCodec,
/// urd/repr/containers/schema_rows.hpp, looked up and handed out as rows of it). Records serialized into an arena have
/// another (ArenaRecords, urd/repr/records/arena.hpp). A storage built with something (its parameter) is built from what
/// the set is built with; one that threads share is appended to under the set's writer lock.
template<typename T, typename Index, bool ThreadSafe, bool Stable, bool Rows, typename Storage>
class IndexedSet
{
    static_assert(!Rows || !std::is_void_v<Storage>, "urd::repr::IndexedSet: rows have a storage of their own");
    static_assert(std::is_void_v<Storage> || Stable, "urd::repr::IndexedSet: a storage of its own never moves its elements (Stable)");
    static_assert(!ThreadSafe || Stable, "urd::repr::IndexedSet: threads share elements that never move (Stable)");
    static_assert(!Rows || Stable, "urd::repr::IndexedSet: rows lie in segments that never move (Stable)");
    friend struct urd::memory_of<IndexedSet>;
    using Elements = std::
        conditional_t<std::is_void_v<Storage>, std::conditional_t<Stable, SegmentedVector<T, default_first_segment_size, ThreadSafe>, std::vector<T>>, Storage>;
    /// What the storage is built with (std::monostate: nothing).
    using Parameter = typename detail::parameter_of<Elements>::type;
    /// How keys hash and compare with the stored elements.
    using Keys = std::conditional_t<Rows, detail::row_keys, detail::element_keys<T>>;
    /// What a set of rows is looked up by besides a T: a range of its elements, or one with its hash.
    template<typename R>
    static constexpr bool row_input = Rows && !std::same_as<R, T> && !std::same_as<R, prehashed<T>> && detail::row_input<R, typename Elements::value_type>;

    /// A position in the vector, as the table holds it: a type of its own, so that it is never taken for an element,
    /// even where the elements are integers too.
    struct Position
    {
        Index value;

        friend bool operator==(Position, Position) = default;
    };
    /// Hashes a position by its element, and a key by the hash it carries, so the table can be searched by key.
    struct PositionHash
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] std::size_t operator()(Position position) const { return Keys::hash((*elements)[position.value]); }
        template<typename K>
        [[nodiscard]] std::size_t operator()(const prehashed<K>& key) const noexcept
        { return key.hash; }
    };
    struct PositionEqual
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] bool operator()(Position lhs, Position rhs) const { return Keys::equal((*elements)[lhs.value], (*elements)[rhs.value]); }
        // The stored element first: its element type is what a row's elements compare as.
        template<typename K>
        [[nodiscard]] bool operator()(const prehashed<K>& lhs, Position rhs) const
        { return Keys::equal((*elements)[rhs.value], lhs.key); }
        template<typename K>
        [[nodiscard]] bool operator()(Position lhs, const prehashed<K>& rhs) const
        { return Keys::equal((*elements)[lhs.value], rhs.key); }
    };
    /// How the table is held and searched: by one thread, or shared by threads.
    using Tables = std::
        conditional_t<ThreadSafe, detail::shared_table<Position, PositionHash, PositionEqual>, detail::single_table<Position, PositionHash, PositionEqual>>;
    using Table = typename Tables::type;

public:
    using iterator = typename Elements::const_iterator;
    using const_iterator = iterator;
    using value_type = T;
    using const_reference = decltype(std::declval<const Elements&>()[std::size_t {}]);  ///< the element, or a row as its storage reads it
    using reference = const_reference;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndexedSet()
        requires std::same_as<Parameter, std::monostate>
    = default;
    /// Rows stored as `parameter` says: elements each in a bit width (a pool of urd::bit), or columns by their widths (a
    /// pool of urd::by_column_widths).
    explicit IndexedSet(const Parameter& parameter)
        requires(!std::same_as<Parameter, std::monostate>)
        : m_parameter(parameter)
    {
    }

    template<std::input_iterator InputIt>
        requires std::same_as<Parameter, std::monostate>
    IndexedSet(InputIt first, InputIt last)
    { insert(first, last); }

    IndexedSet(const IndexedSet& other) : m_parameter(other.m_parameter) { insert(other.begin(), other.end()); }
    IndexedSet& operator=(const IndexedSet& other)
    {
        if (this != &other)
            *this = IndexedSet(other);
        return *this;
    }
    /// The moved-from set is empty.
    IndexedSet(IndexedSet&& other) noexcept : m_parameter(other.m_parameter), m_state(other.release()) {}
    // NOLINTNEXTLINE(bugprone-exception-escape): copying the parameter can throw only std::bad_alloc (column widths past their inline room)
    IndexedSet& operator=(IndexedSet&& other) noexcept
    {
        if (this != &other)
        {
            m_parameter = other.m_parameter;
            delete release();
            detail::store(m_state, other.release());
        }
        return *this;
    }
    ~IndexedSet() { delete release(); }

    [[nodiscard]] iterator begin() const { return elements().begin(); }
    [[nodiscard]] iterator end() const { return elements().end(); }
    [[nodiscard]] size_type size() const { return elements().size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    [[nodiscard]] const T* data() const
        requires(!Stable)
    { return elements().data(); }

    void reserve(size_type count)
        requires(!Stable)
    {
        auto& held = writable_state();
        held.elements.reserve(count);
        held.positions.reserve(count);
    }
    /// As the vector of its elements: how many fit without growing, the most it can hold, and giving back unused room.
    [[nodiscard]] size_type capacity() const
        requires(!Stable)
    { return elements().capacity(); }
    [[nodiscard]] size_type max_size() const
        requires(!Stable)
    { return elements().max_size(); }
    void shrink_to_fit()
        requires(!Stable)
    { writable_state().elements.shrink_to_fit(); }
    // Every lookup and insert takes the element itself or a prehashed<T> (urd/elements/prehashed.hpp), which is not
    // hashed again; either way the element is hashed once, an insert included.
    [[nodiscard]] iterator find(const T& element) const { return found_at(lookup(element)); }
    [[nodiscard]] iterator find(const prehashed<T>& key) const { return found_at(lookup(key)); }
    /// As the standard's unique sets give it: the element found, if any.
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const T& element) const
    {
        const auto found = lookup(element);
        return found ? std::pair { at(*found), std::next(at(*found)) } : std::pair { end(), end() };
    }
    /// A set of rows looks a row up by any contiguous range of its elements.
    template<typename R>
        requires row_input<R>
    [[nodiscard]] iterator find(const R& row) const
    { return found_at(lookup(row)); }
    [[nodiscard]] size_type count(const T& element) const { return contains(element) ? 1 : 0; }
    [[nodiscard]] size_type count(const prehashed<T>& key) const { return contains(key) ? 1 : 0; }
    template<typename R>
        requires row_input<R>
    [[nodiscard]] size_type count(const R& row) const
    { return contains(row) ? 1 : 0; }
    [[nodiscard]] bool contains(const T& element) const { return lookup(element).has_value(); }
    [[nodiscard]] bool contains(const prehashed<T>& key) const { return lookup(key).has_value(); }
    template<typename R>
        requires row_input<R>
    [[nodiscard]] bool contains(const R& row) const
    { return lookup(row).has_value(); }

    /// The element's position, appending it where it is new. Thread-safe, a lookup first, without a lock; a new
    /// element is appended and indexed under the lock, looked up again there in case another thread inserted it.
    std::pair<iterator, bool> insert(const T& element) { return inserted(element, Keys::hash(element)); }
    std::pair<iterator, bool> insert(T&& element)
    {
        const auto hash = Keys::hash(element);
        return inserted(std::move(element), hash);
    }
    /// A copy of the key where it is new.
    std::pair<iterator, bool> insert(const prehashed<T>& key) { return inserted(key.key, key.hash); }
    /// A set of rows inserts a row from any range of its elements its storage takes, or one with its hash
    /// (urd::hash_range of the elements), copied where it is new.
    template<detail::row_of<typename Elements::value_type> R>
        requires row_input<R>
    std::pair<iterator, bool> insert(const R& row)
    { return inserted(row, Keys::hash(row)); }
    template<detail::row_of<typename Elements::value_type> R>
        requires row_input<prehashed<R>>
    std::pair<iterator, bool> insert(const prehashed<R>& key)
    { return inserted(key.key, key.hash); }
    /// New elements go last, wherever the hint is.
    iterator insert(const_iterator /*hint*/, const T& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first,
                      last,
                      [this](const auto& element)
                      {
                          if constexpr (Rows)  // a row as it is handed out (a span, decoded values): copied in
                              this->insert(element);
                          else
                              this->insert(T(element));
                      });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }

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
    /// Later elements move up to keep the order, so their positions change (and a row's span).
    // ponytail: O(n) per erase, indexing again what moves; an interner never erases.
    iterator erase(const_iterator first, const_iterator last)
        requires(!Stable || (Rows && !ThreadSafe))
    {
        const auto from = static_cast<Index>(first - begin());
        erase_positions(from, static_cast<Index>(last - begin()));
        return at(from);
    }
    /// As std::set::merge: moves into this set each element of `source` it lacks; `source` keeps the others.
    // ponytail: O(n) per element moved (erase keeps the order); an interner never merges.
    void merge(IndexedSet& source)
        requires(!Stable && !Rows)
    {
        for (auto it = source.begin(); it != source.end();)
            it = insert(*it).second ? source.erase(it) : std::next(it);
    }
    /// The table of an empty set is left as it is: clearing it costs its capacity, and the thread-safe one locks each of
    /// its 2^7 shards, which a set cleared over and over while empty (a relation layer reset per state) would pay each time.
    void clear()
    {
        if (empty())
            return;
        auto& held = writable_state();
        held.positions.clear();
        held.elements.clear();
    }

    /// Equal elements in the same order.
    [[nodiscard]] friend bool operator==(const IndexedSet& lhs, const IndexedSet& rhs)
    {
        return std::ranges::equal(lhs, rhs, [](const auto& a, const auto& b) { return Keys::equal(a, b); });
    }

private:
    friend struct urd::detail::indexed_access;

    /// The element, or a row's span.
    [[nodiscard]] decltype(auto) operator[](size_type position) const { return elements()[position]; }
    /// What the rows are stored by: their elements' bit width (BitCodec), or their column widths (SchemaCodec).
    [[nodiscard]] const Parameter& built_with() const noexcept
        requires(!std::same_as<Parameter, std::monostate>)
    { return m_parameter; }
    /// The position of `element`, or none. The lookup for a set threads insert into: an iterator from find compared
    /// with end() races with the size, which another thread's insert moves between the two calls.
    [[nodiscard]] std::optional<size_type> position(const T& element) const { return position_from(lookup(element)); }
    [[nodiscard]] std::optional<size_type> position(const prehashed<T>& key) const { return position_from(lookup(key)); }
    template<typename R>
        requires row_input<R>
    [[nodiscard]] std::optional<size_type> position(const R& row) const
    { return position_from(lookup(row)); }
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
        return appended(key, [&](Index position) -> decltype(auto) { return make(static_cast<std::size_t>(position)); });
    }
    /// Erases the element at `position` by moving the last one into its place: O(1) expected, and only the last
    /// element's position changes (to `position`). The order is not kept.
    void swap_erase(Index position)
        requires(!Stable || (Rows && !ThreadSafe))
    {
        assert(position < size());
        const auto last = static_cast<Index>(size() - 1);
        auto& [elements, positions] = writable_state();
        // Room for twice the elements before erasing, so that gtl's table cleans its erased slots in place, never growing
        // (allocating) as elements come and go.
        positions.reserve(size() * 2);
        // The table hashes a position by the element there: each leaves the table before its element moves.
        positions.erase(Position { position });
        if (position != last)
            positions.erase(Position { last });
        detail::swap_erase(elements, position);
        if (position != last)
            positions.insert(Position { position });
    }
    [[nodiscard]] iterator at(Index position) const { return begin() + static_cast<difference_type>(position); }
    [[nodiscard]] iterator found_at(std::optional<Index> found) const { return found ? at(*found) : end(); }
    [[nodiscard]] static std::optional<size_type> position_from(std::optional<Index> found)
    { return found ? std::optional<size_type>(static_cast<size_type>(*found)) : std::nullopt; }
    /// The position of `x`: an element or a row, hashed here; or one with its hash (prehashed), not hashed again.
    template<typename X>
    [[nodiscard]] std::optional<Index> lookup(const X& x) const
    { return lookup(prehashed<X> { x, Keys::hash(x) }); }
    template<typename K>
    [[nodiscard]] std::optional<Index> lookup(const prehashed<K>& key) const
    {
        const auto* held = state();
        return held ? Tables::find(held->positions, key) : std::nullopt;
    }
    /// Erases the elements at [from, to) of a storage that erases in place (a vector), which keeps its capacity: the
    /// elements that move leave the table before they do (it hashes a position by the element there) and go back in after.
    void erase_positions(Index from, Index to)
        requires requires(Elements& elements) { elements.erase(elements.begin(), elements.begin()); }
    {
        auto& [elements, positions] = writable_state();
        for (auto position = from; position < size(); ++position)
            positions.erase(Position { position });
        elements.erase(elements.begin() + from, elements.begin() + to);
        for (auto position = from; position < size(); ++position)
            positions.insert(Position { position });
    }
    /// Stored rows never move: the kept ones go into a new state and are indexed again.
    void erase_positions(Index from, Index to)
    {
        auto& old = writable_state().elements;
        auto kept = std::make_unique<State>(m_parameter);
        for (Index position = 0; position < size(); ++position)
            if (position < from || position >= to)
                kept->elements.push_back(std::move(old[position]));
        delete release();
        m_state = kept.release();
        for (Index position = 0; position < size(); ++position)
            writable_state().positions.insert(Position { position });
    }
    /// `element`, whose hash is `hash`, appended where it is new. A row that does not fit its storage throws here,
    /// before the table is touched.
    template<typename U>
    std::pair<iterator, bool> inserted(U&& element, hash_t hash)
    {
        if constexpr (requires(Elements& rows) { rows.check(element); })
            writable().check(element);
        return appended(prehashed<std::remove_cvref_t<U>> { element, hash }, [&](Index /*position*/) -> decltype(auto) { return std::forward<U>(element); });
    }
    /// The position of `key`: where it is new, the element `make(position)` gives (equal to the key) is appended, as the
    /// table's policy says (Tables): under the writer lock where threads share the set.
    template<typename K, typename Make>
    std::pair<iterator, bool> appended(const prehashed<K>& key, Make make)
    {
        auto& [elements, positions] = writable_state();
        const auto [position, inserted] = Tables::find_or_insert(positions,
                                                                 m_writer,
                                                                 key,
                                                                 [&]
                                                                 {
                                                                     const auto next = static_cast<Index>(elements.size());
                                                                     elements.push_back(make(next));
                                                                     return next;
                                                                 });
        return { at(position), inserted };
    }

    /// What the set owns, at one address (the table hashes and compares through the elements): none until the first insert,
    /// when reads see an empty storage shared by all. Threads make it under the writer lock and find it by a pointer they load.
    struct State
    {
        explicit State(const Parameter& parameter) : elements(make(parameter)) {}

        [[nodiscard]] static Elements make([[maybe_unused]] const Parameter& parameter)
        {
            if constexpr (std::same_as<Parameter, std::monostate>)
                return Elements();
            else
                return Elements(parameter);
        }

        Elements elements;
        Table positions { 0, PositionHash { &elements }, PositionEqual { &elements } };
    };
    [[nodiscard]] static const Elements& no_elements()
    {
        static const Elements empty = State::make(Parameter {});
        return empty;
    }
    [[nodiscard]] const State* state() const noexcept { return detail::load(m_state); }
    [[nodiscard]] const Elements& elements() const
    {
        const auto* held = state();
        return held ? held->elements : no_elements();
    }
    /// The state, made where there is none (threads: by the first of them, under the writer lock).
    State& writable_state()
    {
        auto* held = detail::load(m_state);
        if (held == nullptr)
        {
            const auto lock = std::scoped_lock(m_writer);
            held = detail::load(m_state);
            if (held == nullptr)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): m_state takes it right after and owns it until release()
                held = new State(m_parameter);
                detail::store(m_state, held);
            }
        }
        return *held;
    }
    Elements& writable() { return writable_state().elements; }
    /// Its state, given up: the set is empty then.
    [[nodiscard]] State* release() noexcept
    {
        auto* held = detail::load(m_state);
        detail::store(m_state, nullptr);
        return held;
    }

    /// What the rows are stored by (a bit width, a schema), which the state is made with.
    [[no_unique_address]] Parameter m_parameter {};
    detail::Pointer<ThreadSafe, State> m_state {};
    // A spin lock: the critical section is an append and a table insert, short as yggdrasil's benchmarks of its
    // interners found, where it beat std::mutex.
    [[no_unique_address]] typename Tables::mutex m_writer;
};

}  // namespace urd::repr

namespace urd::detail
{

/// What the interners and pools that hold an indexed set reach of it beyond a Store's interface: the parameter its rows
/// are stored by, an element by position and its position, an insert that builds the element from its position, and an
/// erase that moves the last element into the gap. The members are the set's own, private; this accessor is the one way to them.
struct indexed_access
{
    template<typename Set>
    [[nodiscard]] static decltype(auto) built_with(const Set& set)
    { return set.built_with(); }
    template<typename Set, typename Key>
    [[nodiscard]] static auto position(const Set& set, const Key& key)
    { return set.position(key); }
    template<typename Set, typename Key, typename Factory>
    static auto insert_with(Set& set, const Key& key, Factory make)
    { return set.insert_with(key, std::move(make)); }
    template<typename Set, typename Position>
    static void swap_erase(Set& set, Position position)
    { set.swap_erase(position); }
    /// The element at `position`, or a row's span.
    template<typename Set>
    [[nodiscard]] static decltype(auto) element(const Set& set, std::size_t position)
    { return set[position]; }
};

}  // namespace urd::detail

namespace urd
{

/// Its state: the elements (with their storage) and the table of their positions.
template<typename T, typename Index, bool ThreadSafe, bool Stable, bool Rows, typename Storage>
struct memory_of<repr::IndexedSet<T, Index, ThreadSafe, Stable, Rows, Storage>>
{
    static std::size_t bytes(const repr::IndexedSet<T, Index, ThreadSafe, Stable, Rows, Storage>& held)
    {
        const auto* state = held.state();
        return state ? sizeof(*state) + memory_usage(state->elements) + memory_usage(state->positions) : 0;
    }
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
