// Declarations from yggdrasil, loki, tyr and runir (docs/SURVEY.md), restated as Store requests, with what
// each resolves to. The libraries' index and view types are stood in for by a type that names its identity
// (identifying_members()), from which urd hashes, compares and orders it: nothing is put into namespace std.
// It compiles or it does not: there is nothing to run.

#include "urd/store.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace
{

/// A stand-in for ygg::Index<T> and the views over it: an id that identifies it.
template<typename Tag>
struct Id
{
    std::uint32_t value = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Id::value> {}; }
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

namespace
{

using urd::key;
using urd::Store;
using urd::value;

using urd::detail::FlatHashMapOf;
using urd::detail::TreeMapOf;

// tyr formalism/unification/substitution.hpp: UnorderedMap<ParameterIndex, size_t> m_positions.
using Positions = Store<key<ParameterIndex>, value<std::size_t>>;
static_assert(std::same_as<Positions::representation, FlatHashMapOf<ParameterIndex, std::size_t>>);

// tyr planning/heuristics/lmcut.cpp: UnorderedMap<ActionBinding, Cost> m_action_used_costs, reset per state.
using UsedCosts = Store<key<ActionBinding>, value<double>, urd::retains>;
static_assert(std::same_as<UsedCosts::representation, FlatHashMapOf<ActionBinding, double>>);

// tyr datalog/lifted/solver.cpp: UnorderedMap<GoalAtom, size_t> m_goal_positions, built once.
using GoalPositions = Store<key<GoalAtom>, value<std::size_t>, urd::frozen>;
static_assert(std::same_as<GoalPositions::representation, FlatHashMapOf<GoalAtom, std::size_t>>);

// tyr analysis/listeners.hpp: UnorderedMap<Index<Predicate>, RuleIndexSet> predicates: a set of rules per
// predicate.
using Listeners = Store<key<Predicate>, value<Store<key<Rule>>>>;
static_assert(std::same_as<Listeners::representation, FlatHashMapOf<Predicate, Store<key<Rule>>>>);

// tyr planning/lifted/effect_atom_index.hpp: UnorderedMap<PredicateView, Groups> m_groups, groups written
// once: a map of frozen rows.
using Groups = Store<key<Predicate>, value<Store<value<Rule>, urd::frozen>>>;
static_assert(std::same_as<Groups::representation, FlatHashMapOf<Predicate, Store<value<Rule>, urd::frozen>>>);

// tyr datalog/policies/cost.hpp, CostBuckets: std::map<Cost, Bucket> m_buckets, visited in cost order.
using Buckets = Store<key<double>, value<std::vector<Rule>>, urd::sorted>;
static_assert(std::same_as<Buckets::representation, TreeMapOf<double, std::vector<Rule>>>);

// runir parsers: std::unordered_map<std::string, size_t> m_types, a symbol table.
using Types = Store<key<std::string>, value<std::size_t>>;
static_assert(std::same_as<Types::representation, FlatHashMapOf<std::string, std::size_t>>);

// yggdrasil serialization: std::map<std::vector<std::byte>, Key, ByteStringLess> m_keys, byte strings in
// order.
using Keys = Store<key<std::vector<std::byte>>, value<std::uint32_t>, urd::sorted>;
static_assert(std::same_as<Keys::representation, TreeMapOf<std::vector<std::byte>, std::uint32_t>>);

// tyr datalog/static_literal_decomposition.cpp: UnorderedMap<StaticLiteralIndex,
// optional<vector<StaticLiteralIndex>>> m_decompositions, a memo.
using Decompositions = Store<key<StaticLiteral>, value<std::optional<std::vector<StaticLiteral>>>>;
static_assert(std::same_as<Decompositions::representation, FlatHashMapOf<StaticLiteral, std::optional<std::vector<StaticLiteral>>>>);

// tyr formalism/planning/grounder_decl.hpp: UnorderedMap<Index<RelationBinding>, Index<Axiom>> container,
// growing as bindings are ground.
using Grounded = Store<key<RelationBinding>, value<Axiom>, urd::monotone>;
static_assert(std::same_as<Grounded::representation, FlatHashMapOf<RelationBinding, Axiom>>);

struct AtomTag;
struct ActionTag;
struct StateTag;
struct VertexTag;
using Atom = Id<AtomTag>;
using Action = Id<ActionTag>;
using State = Id<StateTag>;
using Vertex = Id<VertexTag>;

/// A search node: its cost and parent.
struct SearchNode
{
    double cost = 0;
    State parent;
};

// tyr datalog: PredicateFactSets, the facts of a predicate as a set of atom indices.
using FactSet = Store<key<Atom>, urd::dense>;
static_assert(std::same_as<FactSet::representation, urd::repr::DenseSet<Atom>>);

// tyr planning heuristics: FF's markings of actions, cleared for every state.
using Marked = Store<key<Action>, urd::dense, urd::constant_clear>;
static_assert(std::same_as<Marked::representation, urd::repr::SparseSet<Action>>);

// tyr and runir search: the node of each state index, as search proceeds.
using SearchNodes = Store<key<State>, value<SearchNode>, urd::dense>;
static_assert(std::same_as<SearchNodes::representation, urd::repr::IndexMap<State, SearchNode, false>>);

// tyr heuristics: PatternDatabase, a cost per abstract state, computed once.
using PatternDatabase = Store<key<State>, value<double>, urd::dense, urd::frozen>;
static_assert(std::same_as<PatternDatabase::representation, urd::repr::IndexMap<State, double, false>>);

// runir graphs: vector<bool> flags per vertex, as the set of flagged vertices in order.
using Flagged = Store<key<Vertex>, urd::dense, urd::sorted>;
static_assert(std::same_as<Flagged::representation, urd::repr::DenseSet<Vertex>>);

// runir graphs/algorithms/color_refinement.hpp: SignatureInterner, a vector of signatures plus an
// UnorderedMap<Signature, Color> giving each new signature the next color, cleared every round.
using Signature = std::vector<std::uint32_t>;
using Signatures = Store<key<Signature>, urd::positional>;
static_assert(std::same_as<Signatures::representation, urd::repr::IndexedSet<Signature>>);

// loki translators: push_unique(IndexList<T>&, UnorderedSet<View<T>>& seen, value), a list without repeats in the
// order first seen.
using Translated = Store<key<Action>, urd::insertion_ordered>;
static_assert(std::same_as<Translated::representation, urd::repr::IndexedSet<Action>>);

// tyr datalog/policies/annotation.hpp: IndexedAchievers, achievers in a vector plus an UnorderedMap<RuleBinding,
// size_t> of their indices, only ever added to.
using Achievers = Store<key<RelationBinding>, urd::positional, urd::monotone>;
static_assert(std::same_as<Achievers::representation, urd::repr::IndexedSet<RelationBinding>>);

}  // namespace

// Not yet: tyr's postings (vector<UnorderedMap<Index<Object>, dynamic_bitset>>) need postings (M4). The maps
// further up are keyed by indices too; where their sites say so, they become dense requests.
