#pragma once

// What a request implies beyond what it states, as data: each rule says, for some kinds of Store and given a tag the
// request holds, which further tag it holds, and whether the Store provides that one itself whatever its container
// (nothing moves once nothing changes; positions are 0..n-1 in any sequence). The machinery applies the rules in
// order (urd::detail::implied, required_of, urd/property_set.hpp) and knows none of them.

#include "urd/tags.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace urd::detail
{

/// A set of kinds, as a mask; empty means every kind.
using kinds = std::uint16_t;
constexpr kinds kinds_of(std::initializer_list<kind> listed)
{
    kinds result = 0;
    for (const auto each : listed)
        result = static_cast<kinds>(result | (1U << static_cast<unsigned>(each)));
    return result;
}
constexpr bool in_kinds(kinds set, kind of) { return set == 0 || (set & (1U << static_cast<unsigned>(of))) != 0; }

/// One implication: for a Store of one of `of` (every kind where empty) holding `when` (always where it is
/// tag::count), it holds `then` too, unless its category already holds something; `provided` where the Store keeps
/// `then` itself, so that a container need not; `stated` where `then` counts as stated with `when`, so that the
/// request's checks (urd/incompatible.hpp) read it too.
struct implication
{
    kinds of = 0;
    tag when = tag::count;
    tag then = tag::count;
    bool provided = false;
    bool stated = false;
};

inline constexpr kinds sequences = kinds_of({ kind::sequence, kind::multiset });

/// The implications, in the order they apply: an earlier one answers a category first.
inline constexpr auto implications = std::to_array<implication>({
    // monotone_priority is a priority queue's promise, so it makes a priority queue.
    { .when = tag::monotone_priority, .then = tag::by_priority, .stated = true },
    // A sequence is identified by position, and its positions are 0..n-1 whatever holds it.
    { .of = sequences, .then = tag::positional },
    { .of = sequences, .then = tag::functional },
    { .of = sequences, .then = tag::dense, .provided = true },
    // A set with positions keeps them in insertion order, unless it asks for another.
    { .of = kinds_of({ kind::set }), .when = tag::positional, .then = tag::insertion_ordered },
    // A set's keys are its contents, each once; a map holds a value per key, its keys each once, unless it says multi.
    { .of = kinds_of({ kind::set }), .then = tag::unique },
    { .of = kinds_of({ kind::map }), .then = tag::functional },
    { .of = kinds_of({ kind::map }), .when = tag::functional, .then = tag::unique },
    // A partition's classes are over indices.
    { .when = tag::partition, .then = tag::dense },
    // Nothing moves once nothing changes.
    { .when = tag::frozen, .then = tag::stable_refs, .provided = true },
    // What the container must offer: nothing if frozen or of a fixed shape, inserts if it only grows or must stay
    // nonempty, else inserts, erase and clear.
    { .when = tag::frozen, .then = tag::immutable },
    { .when = tag::fixed_shape, .then = tag::immutable },
    { .when = tag::monotone, .then = tag::growable },
    { .when = tag::nonempty, .then = tag::growable },
    { .then = tag::shrinkable },
});

}  // namespace urd::detail
