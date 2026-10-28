#pragma once

#include "urd/elements.hpp"
#include "urd/semantics/equal_to.hpp"
#include "urd/semantics/hash.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace urd::repr
{

/// A set kept in insertion order: the elements in a vector, and a hash table of their positions that hashes and
/// compares through the vector, so each element is stored once. An element's position is its index, which never
/// changes while nothing is erased: an interner. The vector lives on the heap, where the table's hash and equality
/// find it however the set moves. The registry names it for any element type, so it is an unconstrained template;
/// its container_traits declare properties only for hashable elements.
template<typename T, typename Index = std::uint32_t>
class IndexedSet
{
    using Elements = std::vector<T>;

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
    using Table = boost::unordered_flat_set<Position, PositionHash, PositionEqual>;

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
    /// The moved-from set keeps a table and vector of its own, empty.
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

    [[nodiscard]] iterator begin() const { return m_elements->begin(); }
    [[nodiscard]] iterator end() const { return m_elements->end(); }
    [[nodiscard]] size_type size() const { return m_elements->size(); }
    [[nodiscard]] bool empty() const { return m_elements->empty(); }
    [[nodiscard]] const T* data() const { return m_elements->data(); }
    [[nodiscard]] const T& operator[](size_type position) const { return (*m_elements)[position]; }

    void reserve(size_type count)
    {
        m_elements->reserve(count);
        m_positions.reserve(count);
    }
    /// The bytes its storage takes: the vector's capacity and the table's slots (with their metadata byte).
    [[nodiscard]] size_type memory_usage() const { return m_elements->capacity() * sizeof(T) + m_positions.bucket_count() * (sizeof(Position) + 1); }

    [[nodiscard]] iterator find(const T& element) const
    {
        const auto found = m_positions.find(element);
        return found == m_positions.end() ? end() : at(*found);
    }
    [[nodiscard]] size_type count(const T& element) const { return m_positions.contains(element) ? 1 : 0; }
    [[nodiscard]] bool contains(const T& element) const { return m_positions.contains(element); }

    std::pair<iterator, bool> insert(const T& element)
    {
        if (const auto found = m_positions.find(element); found != m_positions.end())
            return { at(*found), false };
        m_elements->push_back(element);
        return appended();
    }
    std::pair<iterator, bool> insert(T&& element)
    {
        if (const auto found = m_positions.find(element); found != m_positions.end())
            return { at(*found), false };
        m_elements->push_back(std::move(element));
        return appended();
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

    size_type erase(const T& element)
    {
        const auto found = m_positions.find(element);
        if (found == m_positions.end())
            return 0;
        erase(at(*found));
        return 1;
    }
    iterator erase(const_iterator pos) { return erase(pos, std::next(pos)); }
    /// Later elements move up to keep the order, so their positions change.
    // ponytail: O(n) per erase, reindexing what follows; an interner never erases.
    iterator erase(const_iterator first, const_iterator last)
    {
        const auto from = static_cast<Index>(first - begin());
        for (auto position = from; position < size(); ++position)
            m_positions.erase(Position { position });
        m_elements->erase(first, last);
        for (auto position = from; position < size(); ++position)
            m_positions.insert(Position { position });
        return at(from);
    }
    void clear()
    {
        m_positions.clear();
        m_elements->clear();
    }

    /// Equal elements in the same order.
    [[nodiscard]] friend bool operator==(const IndexedSet& lhs, const IndexedSet& rhs)
    { return std::ranges::equal(*lhs.m_elements, *rhs.m_elements, EqualTo<T> {}); }

private:
    [[nodiscard]] static Table table_for(const Elements& elements) { return Table(0, PositionHash { &elements }, PositionEqual { &elements }); }
    [[nodiscard]] iterator at(Index position) const { return begin() + static_cast<difference_type>(position); }
    [[nodiscard]] iterator at(Position position) const { return at(position.value); }
    /// Indexes the element just appended.
    std::pair<iterator, bool> appended()
    {
        const auto position = static_cast<Index>(size() - 1);
        m_positions.insert(Position { position });
        return { at(position), true };
    }

    std::unique_ptr<Elements> m_elements = std::make_unique<Elements>();
    Table m_positions = table_for(*m_elements);
};

}  // namespace urd::repr

namespace urd
{

/// In insertion order, in one array; erase and clear keep the storage of both the array and the table.
template<hashable T, typename Index>
struct container_traits<repr::IndexedSet<T, Index>> : properties_of<insertion_ordered, unique, contiguous, shrinkable, retains>
{
};

}  // namespace urd
