#pragma once

#include "urd/interface.hpp"
#include "urd/properties.hpp"
#include "urd/registry.hpp"
#include "urd/traits.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace urd
{

namespace detail
{

/// A Store resolved to representation R for spec S. It offers the standard interface its
/// requested properties allow (interface.hpp), and container_traits reports S's properties, not the
/// representation's, so code cannot come to rely on what it never asked for. Beyond that interface:
/// positions (operator[], at, front, back) for a sequence, and changing elements in place where no
/// property depends on their values and the Store may change.
template<typename R, typename S>
class Store
{
public:
    using representation = R;

private:
    static constexpr bool keyed = by_value<S>;
    static constexpr bool writable = !keyed && !has<S, monotone>;
    static constexpr bool ordered = has<S, sorted> || S::is_sequence;

public:
    using value_type = typename S::element_type;
    using key_type = typename S::key_type;
    using mapped_type = typename S::mapped_type;
    /// What lookups take: a map's key, else an element.
    using lookup_type = std::conditional_t<S::is_map, key_type, value_type>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    // A row of a nested Store may come as a view of flattened storage, so the references are the representation's.
    using const_reference = std::ranges::range_reference_t<const representation>;
    using reference = std::conditional_t<writable, std::ranges::range_reference_t<representation>, const_reference>;
    using const_iterator = std::ranges::iterator_t<const representation>;
    using iterator = std::conditional_t<writable, std::ranges::iterator_t<representation>, const_iterator>;
    static constexpr workload work = S::work;

    Store()
        requires(!has<S, nonempty>)
    = default;

    /// From any elements: a set keeps each once, a sorted sequence sorts them. A nonempty Store needs one.
    template<std::input_iterator InputIt>
    Store(InputIt first, InputIt last) : m_representation(first, last)
    { assert((!has<S, nonempty> || !empty()) && "urd::Store: a nonempty Store needs an element"); }

    Store(std::initializer_list<value_type> elements) : Store(elements.begin(), elements.end()) {}

    /// From elements claimed to have the properties (a sequence's order and uniqueness, any Store's
    /// nonemptiness), so the representation need not establish them; Debug builds check the claim.
    [[nodiscard]] static Store assume(std::vector<value_type> elements)
    {
        if constexpr (S::is_sequence)
            assert(holds<S>(elements) && "urd::Store::assume: the contents lack a claimed property");
        else
            assert((!has<S, nonempty> || !elements.empty()) && "urd::Store::assume: the contents lack a claimed property");
        return Store(std::make_move_iterator(elements.begin()), std::make_move_iterator(elements.end()));
    }

    [[nodiscard]] const_iterator begin() const { return m_representation.begin(); }
    [[nodiscard]] const_iterator end() const { return m_representation.end(); }
    [[nodiscard]] iterator begin()
        requires writable
    { return m_representation.begin(); }
    [[nodiscard]] iterator end()
        requires writable
    { return m_representation.end(); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend() const { return end(); }
    [[nodiscard]] auto rbegin() const
        requires ordered
    { return std::make_reverse_iterator(end()); }
    [[nodiscard]] auto rend() const
        requires ordered
    { return std::make_reverse_iterator(begin()); }
    [[nodiscard]] size_type size() const { return std::ranges::size(m_representation); }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// Hints how many elements will be inserted; does nothing where the representation cannot reserve.
    void reserve(size_type count)
        requires has<S, growable>
    {
        if constexpr (requires { m_representation.reserve(count); })
            m_representation.reserve(count);
    }

    // Positions, for a sequence.

    [[nodiscard]] const_reference operator[](size_type pos) const
        requires(S::is_sequence)
    {
        assert(pos < size());
        return begin()[static_cast<difference_type>(pos)];
    }
    [[nodiscard]] reference operator[](size_type pos)
        requires(S::is_sequence && writable)
    {
        assert(pos < size());
        return begin()[static_cast<difference_type>(pos)];
    }
    [[nodiscard]] const_reference at(size_type pos) const
        requires(S::is_sequence)
    {
        if (pos >= size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    [[nodiscard]] reference at(size_type pos)
        requires(S::is_sequence && writable)
    {
        if (pos >= size())
            throw std::out_of_range("urd::Store::at: position out of range");
        return (*this)[pos];
    }
    /// The first element; a nonempty Store always has one, any other needs one.
    [[nodiscard]] const_reference front() const
        requires(S::is_sequence)
    {
        assert(!empty());
        return *begin();
    }
    [[nodiscard]] reference front()
        requires(S::is_sequence && writable)
    {
        assert(!empty());
        return *begin();
    }
    [[nodiscard]] const_reference back() const
        requires(S::is_sequence)
    {
        assert(!empty());
        return *std::prev(end());
    }
    [[nodiscard]] reference back()
        requires(S::is_sequence && writable)
    {
        assert(!empty());
        return *std::prev(end());
    }
    [[nodiscard]] auto data() const
        requires has<S, contiguous>
    { return std::ranges::cdata(m_representation); }
    [[nodiscard]] value_type* data()
        requires(has<S, contiguous> && writable)
    { return std::ranges::data(m_representation); }

    // Lookup, where elements are kept by value.

    [[nodiscard]] const_iterator find(const lookup_type& element) const
        requires keyed
    { return m_representation.find(element); }
    [[nodiscard]] size_type count(const lookup_type& element) const
        requires keyed
    { return m_representation.count(element); }
    [[nodiscard]] bool contains(const lookup_type& element) const
        requires keyed
    { return m_representation.contains(element); }
    [[nodiscard]] const_iterator lower_bound(const lookup_type& element) const
        requires has<S, sorted>
    { return m_representation.lower_bound(element); }
    [[nodiscard]] const_iterator upper_bound(const lookup_type& element) const
        requires has<S, sorted>
    { return m_representation.upper_bound(element); }
    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const lookup_type& element) const
        requires has<S, sorted>
    { return m_representation.equal_range(element); }

    // Inserting, unless frozen: by value as std::set or std::multiset does, or at the end as std::vector does.

    using insert_result = std::conditional_t<has<S, unique>, std::pair<iterator, bool>, iterator>;

    insert_result insert(const value_type& element)
        requires(keyed && has<S, growable>)
    { return m_representation.insert(element); }
    insert_result insert(value_type&& element)
        requires(keyed && has<S, growable>)
    { return m_representation.insert(std::move(element)); }
    /// With a hint where the element goes.
    iterator insert(const_iterator hint, const value_type& element)
        requires(keyed && has<S, growable>)
    { return m_representation.insert(hint, element); }
    template<std::input_iterator InputIt>
    void insert(InputIt first, InputIt last)
        requires(keyed && has<S, growable>)
    { m_representation.insert(first, last); }
    void insert(std::initializer_list<value_type> elements)
        requires(keyed && has<S, growable>)
    { insert(elements.begin(), elements.end()); }
    template<typename... Args>
    insert_result emplace(Args&&... args)
        requires(keyed && has<S, growable>)
    { return m_representation.emplace(std::forward<Args>(args)...); }

    // A map with unique keys: its values, by key. Values change only through these; iterating gives const elements,
    // as some representations' iterators would let a key be overwritten.

    // The members below are templates only so that their signatures, which name key_type and mapped_type, form
    // for maps alone. K is never deduced (std::type_identity_t): a key is always converted to key_type.

    template<typename K = key_type, typename M = mapped_type>
    [[nodiscard]] const M& at(const std::type_identity_t<K>& key) const
        requires(S::is_map && has<S, unique>)
    { return m_representation.at(key); }
    template<typename K = key_type, typename M = mapped_type>
    [[nodiscard]] M& at(const std::type_identity_t<K>& key)
        requires(S::is_map && has<S, unique> && !has<S, frozen>)
    { return m_representation.at(key); }
    /// The value for `key`, inserted as a default first where there is none.
    template<typename K = key_type, typename M = mapped_type>
    M& operator[](const std::type_identity_t<K>& key)
        requires(S::is_map && has<S, unique> && has<S, growable> && std::default_initializable<M>)
    { return m_representation.try_emplace(key).first->second; }
    /// Inserts a value built from `args` unless `key` has one.
    template<typename K = key_type, typename... Args>
    std::pair<iterator, bool> try_emplace(const std::type_identity_t<K>& key, Args&&... args)
        requires(S::is_map && has<S, unique> && has<S, growable>)
    { return m_representation.try_emplace(key, std::forward<Args>(args)...); }
    /// Sets the value for `key`, inserting it where there is none.
    template<typename M, typename K = key_type>
    std::pair<iterator, bool> insert_or_assign(const std::type_identity_t<K>& key, M&& mapped)
        requires(S::is_map && has<S, unique> && has<S, growable>)
    { return m_representation.insert_or_assign(key, std::forward<M>(mapped)); }

    void push_back(const value_type& element)
        requires(!keyed && has<S, growable>)
    { m_representation.push_back(element); }
    void push_back(value_type&& element)
        requires(!keyed && has<S, growable>)
    { m_representation.push_back(std::move(element)); }
    /// The new element; changeable in place only where the Store's elements are.
    template<typename... Args>
    std::conditional_t<writable, reference, const_reference> emplace_back(Args&&... args)
        requires(!keyed && has<S, growable>)
    { return m_representation.emplace_back(std::forward<Args>(args)...); }
    /// Inserting before a position moves the later elements, so only a sequence that may erase offers it.
    iterator insert(const_iterator pos, const value_type& element)
        requires(!keyed && has<S, shrinkable>)
    { return m_representation.insert(pos, element); }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args)
        requires(!keyed && has<S, shrinkable>)
    { return m_representation.emplace(pos, std::forward<Args>(args)...); }

    // Removing, unless the Store only grows (monotone, frozen) or must keep an element (nonempty). Removing
    // an element ends the references to it.

    iterator erase(const_iterator pos)
        requires has<S, shrinkable>
    {
        // Two conversions, one at a time: the representation's result (perhaps a proxy) to its iterator, that
        // to the Store's.
        const std::ranges::iterator_t<representation> next = m_representation.erase(pos);
        return next;
    }
    iterator erase(const_iterator first, const_iterator last)
        requires has<S, shrinkable>
    { return m_representation.erase(first, last); }
    /// Every element equal to `element` (in a map, with that key); how many there were.
    size_type erase(const lookup_type& element)
        requires(keyed && has<S, shrinkable>)
    { return m_representation.erase(element); }
    void pop_back()
        requires(!keyed && has<S, shrinkable>)
    { m_representation.pop_back(); }
    void clear()
        requires has<S, shrinkable>
    { m_representation.clear(); }

    /// Removes the elements `pred` holds for; how many. Found by argument-dependent lookup, as std::erase_if.
    template<typename Pred>
    friend size_type erase_if(Store& store, Pred pred)
        requires has<S, shrinkable>
    {
        const auto before = store.size();
        if constexpr (writable)
        {
            auto& range = store.m_representation;
            range.erase(std::remove_if(range.begin(), range.end(), pred), range.end());
        }
        else
            // ponytail: one erase per element, O(n²) on a flat set; erase_if of the representation if it matters
            for (auto it = store.begin(); it != store.end();)
                it = pred(*it) ? store.erase(it) : std::next(it);
        return before - store.size();
    }
    /// Every element equal to `element`, from a sequence; how many. As std::erase.
    friend size_type erase(Store& store, const value_type& element)
        requires(!keyed && has<S, shrinkable>)
    {
        return erase_if(store, [&](const value_type& candidate) { return candidate == element; });
    }

    void swap(Store& other) noexcept
    {
        using std::swap;
        swap(m_representation, other.m_representation);
    }
    friend void swap(Store& lhs, Store& rhs) noexcept { lhs.swap(rhs); }

    [[nodiscard]] friend bool operator==(const Store& lhs, const Store& rhs)
        requires std::equality_comparable<value_type>
    {
        if constexpr (std::equality_comparable<representation>)
            return lhs.m_representation == rhs.m_representation;
        else
            return std::ranges::equal(lhs, rhs);
    }
    /// In order, as the standard's ordered containers compare.
    [[nodiscard]] friend auto operator<=>(const Store& lhs, const Store& rhs)
        requires(ordered && std::three_way_comparable<value_type>)
    { return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }

private:
    representation m_representation;
};

template<typename... Ps>
struct resolve
{
    using spec_type = typename to_spec<Ps...>::type;
    using type = Store<typename choose<spec_type>::type, spec_type>;
};

}  // namespace detail

/// A data structure described by its properties. List what the data is and how it is used, in any order:
///
///     using Ids = urd::Store<urd::key<int>, urd::sorted>;            // a sorted set of ints
///     using Log = urd::Store<urd::value<Event>, urd::monotone>;      // a sequence that only grows
///
/// urd picks the representation; Store<…>::representation names it, for tests that pin it.
template<typename... Ps>
using Store = typename detail::resolve<Ps...>::type;

/// A Store has the properties it was asked for, not those of its representation.
template<typename R, typename S>
struct container_traits<detail::Store<R, S>> : S
{
};

}  // namespace urd
