#pragma once

#include "urd/elements.hpp"
#include "urd/repr/containers/segmented_vector.hpp"
#include "urd/repr/indexed.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/concurrent_flat_set.hpp>
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

/// An IndexedSet that threads may insert into at once, and read from while others insert: an interner shared across
/// threads (tyr's state storages), and the node table of a thread-safe tree pool. The elements lie in a thread-safe
/// SegmentedVector, whose elements never move, and a concurrent hash table holds their positions, hashing and
/// comparing through it. Looking up is lock-free; an insert of a new element takes a lock, so positions stay 0..n-1
/// without gaps. Not covered: clear, copy and move, which need no other operation running.
template<typename T, typename Index = std::uint32_t>
class ConcurrentIndexedSet
{
    using Elements = SegmentedVector<T, default_first_segment_size, true>;

    /// A position in the vector, as the table holds it: a type of its own, never taken for an element.
    struct Position
    {
        Index value;

        friend bool operator==(Position, Position) = default;
    };
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
    using Table = boost::concurrent_flat_set<Position, PositionHash, PositionEqual>;

public:
    using iterator = typename Elements::const_iterator;
    using const_iterator = iterator;
    using value_type = T;
    using key_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    ConcurrentIndexedSet() = default;

    template<std::input_iterator InputIt>
    ConcurrentIndexedSet(InputIt first, InputIt last)
    { insert(first, last); }

    ConcurrentIndexedSet(const ConcurrentIndexedSet& other) : ConcurrentIndexedSet(other.begin(), other.end()) {}
    ConcurrentIndexedSet& operator=(const ConcurrentIndexedSet& other)
    {
        if (this != &other)
            *this = ConcurrentIndexedSet(other);
        return *this;
    }
    /// The moved-from set keeps a table and elements of its own, empty.
    ConcurrentIndexedSet(ConcurrentIndexedSet&& other) noexcept :
        m_elements(std::exchange(other.m_elements, std::make_unique<Elements>())),
        m_positions(std::exchange(other.m_positions, table_for(*other.m_elements)))
    {
    }
    ConcurrentIndexedSet& operator=(ConcurrentIndexedSet&& other) noexcept
    {
        if (this != &other)
        {
            m_elements = std::exchange(other.m_elements, std::make_unique<Elements>());
            m_positions = std::exchange(other.m_positions, table_for(*other.m_elements));
        }
        return *this;
    }
    ~ConcurrentIndexedSet() = default;

    [[nodiscard]] iterator begin() const { return m_elements->cbegin(); }
    [[nodiscard]] iterator end() const { return m_elements->cend(); }
    [[nodiscard]] size_type size() const { return m_elements->size(); }
    [[nodiscard]] bool empty() const { return size() == 0; }
    [[nodiscard]] const T& operator[](size_type position) const { return (*m_elements)[position]; }

    /// The bytes its storage takes: the segments, and the table's elements (its slack is not counted).
    [[nodiscard]] size_type memory_usage() const { return m_elements->memory_usage() + m_positions.size() * (sizeof(Position) + 1); }

    [[nodiscard]] iterator find(const T& element) const
    {
        const auto found = position_of(element);
        return found ? at(*found) : end();
    }
    [[nodiscard]] size_type count(const T& element) const { return position_of(element) ? 1 : 0; }
    [[nodiscard]] bool contains(const T& element) const { return position_of(element).has_value(); }

    /// The element's position, inserting it last where it is new. A lookup first, without a lock; a new element is
    /// appended and indexed under the lock, looked up again there in case another thread inserted it meanwhile.
    std::pair<iterator, bool> insert(const T& element)
    {
        if (const auto found = position_of(element))
            return { at(*found), false };
        const auto lock = std::scoped_lock(m_writer);
        if (const auto found = position_of(element))
            return { at(*found), false };
        const auto position = static_cast<Index>(m_elements->size());
        m_elements->push_back(element);
        m_positions.insert(Position { position });
        return { at(position), true };
    }
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

    /// Not concurrent with anything.
    void clear()
    {
        m_positions.clear();
        m_elements->clear();
    }

private:
    [[nodiscard]] static Table table_for(const Elements& elements) { return Table(0, PositionHash { &elements }, PositionEqual { &elements }); }
    [[nodiscard]] iterator at(Index position) const { return begin() + static_cast<difference_type>(position); }
    [[nodiscard]] std::optional<Index> position_of(const T& element) const
    {
        auto found = std::optional<Index>();
        m_positions.cvisit(element, [&found](const Position& position) { found = position.value; });
        return found;
    }

    std::unique_ptr<Elements> m_elements = std::make_unique<Elements>();
    Table m_positions = table_for(*m_elements);
    std::mutex m_writer;
};

/// The table a pool interns its rows or nodes in: thread-safe where ThreadSafe.
template<typename T, bool ThreadSafe>
using InterningTable = std::conditional_t<ThreadSafe, ConcurrentIndexedSet<T>, IndexedSet<T>>;

}  // namespace urd::repr

namespace urd
{

/// In insertion order, never moved, only ever added to, by threads at once.
template<hashable T, typename Index>
struct container_traits<repr::ConcurrentIndexedSet<T, Index>> : properties_of<insertion_ordered, unique, stable_refs, growable, retains, concurrent>
{
};

}  // namespace urd
