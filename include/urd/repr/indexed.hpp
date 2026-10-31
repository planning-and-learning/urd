#pragma once

#include "urd/elements.hpp"
#include "urd/registry/row.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/containers/spin_mutex.hpp"
#include "urd/repr/indexed_fwd.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd::repr
{

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
template<typename T, typename Index, bool ThreadSafe, bool Stable>
class IndexedSet
{
    static_assert(!ThreadSafe || Stable, "urd::repr::IndexedSet: threads share elements that never move (Stable)");
    using Elements = std::conditional_t<Stable, SegmentedVector<T, default_first_segment_size, ThreadSafe>, std::vector<T>>;

    /// A position in the vector, as the table holds it: a type of its own, so that it is never taken for an element,
    /// even where the elements are integers too.
    struct Position
    {
        Index value;

        friend bool operator==(Position, Position) = default;
    };

    /// Hashes a position by its element, and an element as itself, so the table can be searched by element.
    struct PositionHash
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] std::size_t operator()(Position position) const { return Hash<T> {}((*elements)[position.value]); }
        [[nodiscard]] std::size_t operator()(const T& element) const { return Hash<T> {}(element); }
    };
    struct PositionEqual
    {
        using is_transparent = void;
        const Elements* elements = nullptr;

        [[nodiscard]] bool operator()(Position lhs, Position rhs) const { return EqualTo<T> {}((*elements)[lhs.value], (*elements)[rhs.value]); }
        [[nodiscard]] bool operator()(const T& lhs, Position rhs) const { return EqualTo<T> {}(lhs, (*elements)[rhs.value]); }
        [[nodiscard]] bool operator()(Position lhs, const T& rhs) const { return EqualTo<T> {}((*elements)[lhs.value], rhs); }
        [[nodiscard]] bool operator()(const T& lhs, const T& rhs) const { return EqualTo<T> {}(lhs, rhs); }
    };
    using Table = std::conditional_t<ThreadSafe,
                                     boost::concurrent_flat_set<Position, PositionHash, PositionEqual>,
                                     boost::unordered_flat_set<Position, PositionHash, PositionEqual>>;

public:
    using iterator = typename Elements::const_iterator;
    using const_iterator = iterator;
    using value_type = T;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndexedSet() = default;

    template<std::input_iterator InputIt>
    IndexedSet(InputIt first, InputIt last)
    { insert(first, last); }

    IndexedSet(const IndexedSet& other) : IndexedSet(other.begin(), other.end()) {}
    IndexedSet& operator=(const IndexedSet& other)
    {
        if (this != &other)
            *this = IndexedSet(other);
        return *this;
    }
    /// The moved-from set keeps a table and elements of its own, empty.
    IndexedSet(IndexedSet&& other) noexcept :
        m_elements(std::exchange(other.m_elements, std::make_unique<Elements>())),
        m_positions(std::exchange(other.m_positions, table_for(*other.m_elements)))
    {
    }
    IndexedSet& operator=(IndexedSet&& other) noexcept
    {
        if (this != &other)
        {
            m_elements = std::exchange(other.m_elements, std::make_unique<Elements>());
            m_positions = std::exchange(other.m_positions, table_for(*other.m_elements));
        }
        return *this;
    }
    ~IndexedSet() = default;

    [[nodiscard]] iterator begin() const { return m_elements->cbegin(); }
    [[nodiscard]] iterator end() const { return m_elements->cend(); }
    [[nodiscard]] size_type size() const { return m_elements->size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    [[nodiscard]] const T& operator[](size_type position) const { return (*m_elements)[position]; }
    [[nodiscard]] const T* data() const
        requires(!Stable)
    { return m_elements->data(); }

    void reserve(size_type count)
        requires(!Stable)
    {
        m_elements->reserve(count);
        m_positions.reserve(count);
    }
    /// The bytes its storage takes: the elements' storage (segments, or the vector's capacity), and the table's
    /// slots (with their metadata byte; a concurrent table counts its elements only, not its slack).
    [[nodiscard]] size_type memory_usage() const
    {
        const auto elements = [this]
        {
            if constexpr (Stable)
                return m_elements->memory_usage();
            else
                return m_elements->capacity() * sizeof(T);
        }();
        if constexpr (ThreadSafe)
            return elements + m_positions.size() * (sizeof(Position) + 1);
        else
            return elements + m_positions.bucket_count() * (sizeof(Position) + 1);
    }

    [[nodiscard]] iterator find(const T& element) const
    {
        const auto found = position_of(element);
        return found ? at(*found) : end();
    }
    /// The position of `element`, or none. The lookup for a set threads insert into: an iterator from find compared
    /// with end() races with the size, which another thread's insert moves between the two calls.
    [[nodiscard]] std::optional<size_type> position(const T& element) const
    {
        const auto found = position_of(element);
        return found ? std::optional<size_type>(static_cast<size_type>(*found)) : std::nullopt;
    }
    [[nodiscard]] size_type count(const T& element) const { return position_of(element) ? 1 : 0; }
    [[nodiscard]] bool contains(const T& element) const { return position_of(element).has_value(); }

    /// The element's position, appending it where it is new. Thread-safe, a lookup first, without a lock; a new
    /// element is appended and indexed under the lock, looked up again there in case another thread inserted it.
    std::pair<iterator, bool> insert(const T& element) { return inserted(element); }
    std::pair<iterator, bool> insert(T&& element) { return inserted(std::move(element)); }
    /// New elements go last, wherever the hint is.
    iterator insert(const_iterator /*hint*/, const T& element) { return insert(element).first; }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](const auto& element) { this->insert(T(element)); });
    }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(T(std::forward<Args>(args)...)); }
    /// The position of `key`, appending what `make(position)` gives where it is new: the factory runs under the
    /// writer lock with the position reserved, for an element that holds its own position, and must give an element
    /// equal to `key`. It must not touch this set.
    template<std::invocable<std::size_t> Factory>
    std::pair<iterator, bool> insert_with(const T& key, Factory make)
    {
        if (const auto found = position_of(key))
            return { at(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        if constexpr (ThreadSafe)
            if (const auto found = position_of(key))
                return { at(*found), false };
        const auto position = static_cast<Index>(size());
        m_elements->push_back(make(static_cast<std::size_t>(position)));
        assert(EqualTo<T> {}((*m_elements)[position], key) && "urd::repr::IndexedSet::insert_with: the factory must give an element equal to the key");
        m_positions.insert(Position { position });
        return { at(position), true };
    }

    size_type erase(const T& element)
        requires(!Stable)
    {
        const auto found = position_of(element);
        if (!found)
            return 0;
        erase(at(*found));
        return 1;
    }
    iterator erase(const_iterator pos)
        requires(!Stable)
    { return erase(pos, std::next(pos)); }
    /// Later elements move up to keep the order, so their positions change.
    // ponytail: O(n) per erase, reindexing what follows; an interner never erases.
    iterator erase(const_iterator first, const_iterator last)
        requires(!Stable)
    {
        const auto from = static_cast<Index>(first - begin());
        for (auto position = from; position < size(); ++position)
            m_positions.erase(Position { position });
        m_elements->erase(first, last);
        for (auto position = from; position < size(); ++position)
            m_positions.insert(Position { position });
        return at(from);
    }
    /// Erases the element at `position` by moving the last one into its place: O(1) expected, and only the last
    /// element's position changes (to `position`). The order is not kept.
    void swap_erase(Index position)
        requires(!Stable)
    {
        assert(position < size());
        const auto last = static_cast<Index>(size() - 1);
        // The table hashes a position by the element there: each leaves the table before its element moves.
        m_positions.erase(Position { position });
        if (position != last)
        {
            m_positions.erase(Position { last });
            (*m_elements)[position] = std::move((*m_elements)[last]);
        }
        m_elements->pop_back();
        if (position != last)
            m_positions.insert(Position { position });
    }
    void clear()
    {
        m_positions.clear();
        m_elements->clear();
    }

    /// Equal elements in the same order.
    [[nodiscard]] friend bool operator==(const IndexedSet& lhs, const IndexedSet& rhs) { return std::ranges::equal(lhs, rhs, EqualTo<T> {}); }

private:
    [[nodiscard]] static Table table_for(const Elements& elements) { return Table(0, PositionHash { &elements }, PositionEqual { &elements }); }
    [[nodiscard]] iterator at(Index position) const { return begin() + static_cast<difference_type>(position); }
    [[nodiscard]] std::optional<Index> position_of(const T& element) const
    {
        if constexpr (ThreadSafe)
        {
            auto found = std::optional<Index>();
            m_positions.cvisit(element, [&found](const Position& position) { found = position.value; });
            return found;
        }
        else
        {
            const auto found = m_positions.find(element);
            return found == m_positions.end() ? std::nullopt : std::optional<Index>(found->value);
        }
    }
    template<typename U>
    std::pair<iterator, bool> inserted(U&& element)
    {
        if (const auto found = position_of(element))
            return { at(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        if constexpr (ThreadSafe)
            if (const auto found = position_of(element))
                return { at(*found), false };
        const auto position = static_cast<Index>(size());
        m_elements->push_back(std::forward<U>(element));
        m_positions.insert(Position { position });
        return { at(position), true };
    }

    std::unique_ptr<Elements> m_elements = std::make_unique<Elements>();
    Table m_positions = table_for(*m_elements);
    [[no_unique_address]] std::conditional_t<ThreadSafe, std::mutex, NoLock> m_writer;
};

}  // namespace urd::repr

namespace urd
{

}  // namespace urd

namespace urd
{

template<>
struct include<"urd/repr/indexed.hpp">
{
};

}  // namespace urd

namespace urd::detail
{

template<typename S>
struct representation_of<"indexed_set", S>
{
    using type = repr::IndexedSet<typename S::element_type>;
};
template<typename S>
struct representation_of<"stable_indexed_set", S>
{
    using type = repr::StableIndexedSet<typename S::element_type>;
};
template<typename S>
struct representation_of<"concurrent_indexed_set", S>
{
    using type = repr::ConcurrentIndexedSet<typename S::element_type>;
};

}  // namespace urd::detail
