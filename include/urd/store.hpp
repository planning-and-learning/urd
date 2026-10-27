#pragma once

#include "urd/properties.hpp"
#include "urd/props.hpp"
#include "urd/registry.hpp"
#include "urd/repr/ops.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace urd
{

namespace detail
{

/// A Store resolved to representation Candidate::type for spec S. It offers exactly what S asks for:
/// operations the requested properties allow, and properties_of reports S's properties, not the
/// representation's, so code cannot come to rely on what it never asked for.
template<typename Candidate, typename S>
class Store
{
public:
    using representation = typename Candidate::type;
    using element_type = typename S::element_type;
    using key_type = typename S::key_type;
    using value_type = typename S::value_type;
    using const_iterator = typename representation::const_iterator;
    static constexpr props properties = S::flags;
    static constexpr workload work = S::work;
    static constexpr std::string_view representation_name = Candidate::name;

    Store()
        requires(!includes(S::flags, props::nonempty))
    = default;

    /// From any elements: a set keeps each once, a sorted sequence sorts them.
    [[nodiscard]] static Store make(std::vector<element_type> elements)
        requires(!includes(S::flags, props::nonempty))
    {
        return Store(ops::make(std::move(elements)));
    }

    /// From elements claimed to have the properties (for a sequence: sorted, unique; for any Store:
    /// nonempty); Debug builds check the claim.
    [[nodiscard]] static Store assume(std::vector<element_type> elements)
    {
        auto made = [&]
        {
            // A set is built from any elements; only a sequence's order and uniqueness are claims.
            if constexpr (S::is_sequence)
                return ops::assume(std::move(elements));
            else
                return ops::make(std::move(elements));
        }();
        assert(holds<S::flags & props::nonempty>(made) && "urd::Store::assume: the contents lack a claimed property");
        return Store(std::move(made));
    }

    [[nodiscard]] const_iterator begin() const { return m_representation.begin(); }
    [[nodiscard]] const_iterator end() const { return m_representation.end(); }
    [[nodiscard]] std::size_t size() const { return m_representation.size(); }
    [[nodiscard]] bool empty() const { return m_representation.empty(); }

    /// The element at `pos` of a sequence.
    [[nodiscard]] const element_type& operator[](std::size_t pos) const
        requires(S::is_sequence)
    {
        return m_representation[pos];
    }

    /// The first element; only a nonempty Store has one, so no check is needed.
    [[nodiscard]] const element_type& front() const
        requires(includes(S::flags, props::nonempty))
    {
        return *m_representation.begin();
    }

    [[nodiscard]] bool contains(const element_type& element) const
        requires(S::is_set)
    {
        return ops::contains(m_representation, element);
    }

    /// Inserts where the properties allow; returns whether `element` was inserted (a set, or a unique
    /// sequence, refuses one it already has). A frozen Store never changes.
    bool insert(const element_type& element)
        requires(!includes(S::flags, props::frozen))
    {
        return ops::insert(m_representation, element);
    }

    /// Removes every element equal to `element`; returns how many. Not offered where removing is
    /// excluded: frozen, monotone (only grows) and nonempty (could become empty).
    std::size_t erase(const element_type& element)
        requires(!includes(S::flags, props::frozen) && !includes(S::flags, props::monotone)
                 && !includes(S::flags, props::nonempty))
    {
        return ops::erase(m_representation, element);
    }

    [[nodiscard]] friend bool operator==(const Store& lhs, const Store& rhs)
        requires std::equality_comparable<representation>
    {
        return lhs.m_representation == rhs.m_representation;
    }

private:
    using ops = repr::ops<representation>;

    explicit Store(representation made) : m_representation(std::move(made)) {}

    representation m_representation;
};

template<typename... Ps>
struct resolve
{
    using spec_type = typename to_spec<Ps...>::type;
    using type = Store<typename registry<spec_type>::type, spec_type>;
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

template<typename Candidate, typename S>
struct properties_of<detail::Store<Candidate, S>>
{
    static constexpr props value = S::flags;
};

}  // namespace urd
