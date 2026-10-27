// Declarations from yggdrasil, loki, tyr and runir (docs/SURVEY.md), restated as Store requests, with what
// each resolves to. The libraries' index and view types are stood in for by types with what they offer here:
// an id compared and hashed. It compiles or it does not: there is nothing to run.

#include "urd/store.hpp"

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{

/// A stand-in for ygg::Index<T> and the views over it: an id, compared and hashed.
template<typename Tag>
struct Id
{
    std::uint32_t value = 0;
    friend bool operator==(const Id&, const Id&) = default;
    friend auto operator<=>(const Id&, const Id&) = default;
};

struct ParameterTag;
struct ActionBindingTag;
struct GoalAtomTag;
struct PredicateTag;
struct RuleTag;
struct StaticLiteralTag;
struct RelationBindingTag;
struct AxiomTag;

using ParameterIndex = Id<ParameterTag>;
using ActionBinding = Id<ActionBindingTag>;
using GoalAtom = Id<GoalAtomTag>;
using Predicate = Id<PredicateTag>;
using Rule = Id<RuleTag>;
using StaticLiteral = Id<StaticLiteralTag>;
using RelationBinding = Id<RelationBindingTag>;
using Axiom = Id<AxiomTag>;

}  // namespace

template<typename Tag>
struct std::hash<Id<Tag>>
{
    std::size_t operator()(const Id<Tag>& id) const noexcept { return std::hash<std::uint32_t> {}(id.value); }
};

namespace
{

using urd::key;
using urd::Store;
using urd::value;

template<typename K, typename V>
using FlatHashMap = boost::unordered_flat_map<K, V, std::hash<K>>;

// tyr formalism/unification/substitution.hpp: UnorderedMap<ParameterIndex, size_t> m_positions.
using Positions = Store<key<ParameterIndex>, value<std::size_t>>;
static_assert(std::same_as<Positions::representation, FlatHashMap<ParameterIndex, std::size_t>>);

// tyr planning/heuristics/lmcut.cpp: UnorderedMap<ActionBinding, Cost> m_action_used_costs, reset per state.
using UsedCosts = Store<key<ActionBinding>, value<double>, urd::retains>;
static_assert(std::same_as<UsedCosts::representation, FlatHashMap<ActionBinding, double>>);

// tyr datalog/lifted/solver.cpp: UnorderedMap<GoalAtom, size_t> m_goal_positions, built once.
using GoalPositions = Store<key<GoalAtom>, value<std::size_t>, urd::frozen>;
static_assert(std::same_as<GoalPositions::representation, FlatHashMap<GoalAtom, std::size_t>>);

// tyr analysis/listeners.hpp: UnorderedMap<Index<Predicate>, RuleIndexSet> predicates: a set of rules per
// predicate.
using Listeners = Store<key<Predicate>, value<Store<key<Rule>>>>;
static_assert(std::same_as<Listeners::representation, FlatHashMap<Predicate, Store<key<Rule>>>>);

// tyr planning/lifted/effect_atom_index.hpp: UnorderedMap<PredicateView, Groups> m_groups, groups written
// once: a map of frozen rows.
using Groups = Store<key<Predicate>, value<Store<value<Rule>, urd::frozen>>>;
static_assert(std::same_as<Groups::representation, FlatHashMap<Predicate, Store<value<Rule>, urd::frozen>>>);

// tyr datalog/policies/cost.hpp, CostBuckets: std::map<Cost, Bucket> m_buckets, visited in cost order.
using Buckets = Store<key<double>, value<std::vector<Rule>>, urd::sorted>;
static_assert(std::same_as<Buckets::representation, std::map<double, std::vector<Rule>>>);

// runir parsers: std::unordered_map<std::string, size_t> m_types, a symbol table.
using Types = Store<key<std::string>, value<std::size_t>>;
static_assert(std::same_as<Types::representation, FlatHashMap<std::string, std::size_t>>);

// yggdrasil serialization: std::map<std::vector<std::byte>, Key, ByteStringLess> m_keys, byte strings in
// order.
using Keys = Store<key<std::vector<std::byte>>, value<std::uint32_t>, urd::sorted>;
static_assert(std::same_as<Keys::representation, std::map<std::vector<std::byte>, std::uint32_t>>);

// tyr datalog/static_literal_decomposition.cpp: UnorderedMap<StaticLiteralIndex,
// optional<vector<StaticLiteralIndex>>> m_decompositions, a memo.
using Decompositions = Store<key<StaticLiteral>, value<std::optional<std::vector<StaticLiteral>>>>;
static_assert(std::same_as<Decompositions::representation, FlatHashMap<StaticLiteral, std::optional<std::vector<StaticLiteral>>>>);

// tyr formalism/planning/grounder_decl.hpp: UnorderedMap<Index<RelationBinding>, Index<Axiom>> container,
// growing as bindings are ground.
using Grounded = Store<key<RelationBinding>, value<Axiom>, urd::monotone>;
static_assert(std::same_as<Grounded::representation, FlatHashMap<RelationBinding, Axiom>>);

}  // namespace

// Not yet: the keys above are dense indices, which M1's dense maps will store as vectors; tyr's postings
// (vector<UnorderedMap<Index<Object>, dynamic_bitset>>) need dense sets (M1) and postings (M4).
