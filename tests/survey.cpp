// Declarations from yggdrasil, loki, tyr and runir (docs/SURVEY.md), restated as Store requests, with what
// each resolves to. The libraries' index and view types are stood in for by a type that names its identity
// (identifying_members()), from which urd hashes, compares and orders it: nothing is put into namespace std.
// It compiles or it does not: there is nothing to run.

#include "urd/compose/family.hpp"
#include "urd/store.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
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
template<typename T>
using FlatHashSetOf = urd::detail::FlatHashSet<T>;
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

// tyr planning/lifted/effect_atom_index.hpp: the effect groups of each predicate, written once; where predicates
// are indices, a frozen multimap by dense key.
using GroupsByIndex = Store<key<Predicate>, value<Rule>, urd::multi, urd::dense, urd::frozen>;
static_assert(std::same_as<GroupsByIndex::representation, urd::repr::CsrMultiMap<Predicate, Rule>>);

// runir graphs/static_graph.hpp: StaticGraph, out-edge offsets built by a counting sort over (source, target)
// pairs; with bidirectional, the in-edges the same way.
using Successors = Store<key<Vertex>, value<Vertex>, urd::multi, urd::dense, urd::frozen>;
static_assert(std::same_as<Successors::representation, urd::repr::CsrMultiMap<Vertex, Vertex>>);
using StaticGraph = Store<key<Vertex>, value<Vertex>, urd::multi, urd::dense, urd::frozen, urd::bidirectional>;
static_assert(std::same_as<StaticGraph::representation, urd::repr::BidirectionalCsr<Vertex, Vertex>>);

// yggdrasil database/semantics/join_index.hpp: JoinIndex, UnorderedMultiMap<hash_t, size_t> m_index, the rows of
// each key hash: postings.
using JoinPostings = Store<key<std::uint64_t>, value<std::size_t>, urd::multi>;
static_assert(std::same_as<JoinPostings::representation, urd::repr::PostingMap<std::uint64_t, std::size_t>>);

struct ObjectTag;
struct RowTag;
using Object = Id<ObjectTag>;
using Row = Id<RowTag>;

// tyr invariants: vector<UnorderedMap<Index<Object>, dynamic_bitset>>, per parameter the rows of each object as a
// bitset: per parameter, a map from objects to dense sets of rows.
using ObjectPostings = Store<value<Store<key<Object>, value<Store<key<Row>, urd::dense>>>>>;
static_assert(std::same_as<ObjectPostings::representation::value_type::representation, FlatHashMapOf<Object, Store<key<Row>, urd::dense>>>);

// yggdrasil database/semantics/search/a_star.hpp: OpenList, a binary heap of (f, g, push, vertex) entries in one
// vector, cleared and reused. Its tie-breaking (larger g first) is an order of its own; here, urd::Less on the
// members as named.
struct OpenEntry
{
    std::uint32_t f = 0;
    std::uint32_t g = 0;
    Vertex vertex;

    static constexpr auto identifying_members() noexcept { return urd::Members<&OpenEntry::f, &OpenEntry::g, &OpenEntry::vertex> {}; }
};
using OpenList = Store<value<OpenEntry>, urd::by_priority, urd::retains>;
static_assert(std::same_as<OpenList::representation, urd::repr::Heap<OpenEntry>>);

// tyr planning/algorithms/openlists/priority_queue.hpp: std::priority_queue<E, vector<E>, EntryComparator>, the
// least key first.
using PriorityOpenList = Store<value<std::pair<double, State>>, urd::by_priority>;
static_assert(std::same_as<PriorityOpenList::representation, urd::repr::Heap<std::pair<double, State>>>);

// Breadth-first search in tyr and runir: a queue of states, first in, first out.
using Frontier = Store<value<State>, urd::fifo>;
static_assert(std::same_as<Frontier::representation, urd::repr::Fifo<State>>);

// loki semantic/translator/common.hpp: TranslationStorage, ~40 UnorderedMap<View<T>, View<T>> per repository,
// with a parent storage whose translations a problem's storage reuses: maps layered over their parent's.
using Translations = Store<key<Predicate>, value<Predicate>, urd::layered>;
static_assert(std::same_as<Translations::representation, urd::repr::Layered<FlatHashMapOf<Predicate, Predicate>>>);

// loki and runir parsers: name-to-entity maps per scope (parameters of an action, variables of a quantifier), the
// innermost binding first.
using Bindings = Store<key<std::string>, value<ParameterIndex>, urd::scoped>;
static_assert(std::same_as<Bindings::representation, urd::repr::ScopedMap<std::string, ParameterIndex>>);

// tyr DenseRelationMap: (relation, row) -> value, cleared by generation per state. Nested dense maps compose: the
// outer one cleared in O(1), each relation's row map by position.
using RowValues = Store<key<RelationBinding>, value<double>, urd::dense>;
using RelationValues = Store<key<Predicate>, value<RowValues>, urd::dense, urd::constant_clear>;
static_assert(std::same_as<RelationValues::representation, urd::repr::IndexMap<Predicate, RowValues, true>>);
static_assert(std::same_as<RowValues::representation, urd::repr::IndexMap<RelationBinding, double, false>>);

// tyr formalism/planning/invariants/constraints.cpp: DisjointSet over term ids, classes of equal terms.
using TermClasses = Store<key<std::size_t>, urd::partition>;
static_assert(std::same_as<TermClasses::representation, urd::repr::DisjointSets<std::size_t>>);

// tyr and runir semi-naive evaluation: the facts of a relation, and those new since the last round.
using Facts = Store<key<Atom>, urd::change_log>;
static_assert(std::same_as<Facts::representation, urd::repr::ChangeLogged<FlatHashSetOf<Atom>>>);

// loki TranslationStorage, tyr static and fluent twins: one Store per kind of entity, reached by its type.
template<typename T>
using Interned = Store<key<T>, urd::positional, urd::monotone>;
using Repositories = urd::compose::Family<Interned, Object, Predicate, Action>;
static_assert(std::same_as<decltype(std::declval<Repositories&>().get<Predicate>()), Interned<Predicate>&>);

// tyr planning/state_repository.hpp: SharedObjectPool<Builder<State>, true>, scratch state builders handed out to
// successor generation across threads and returned: slots of buffers, acquired as they were left.
using StateBuilders = Store<value<std::vector<std::uint32_t>>, urd::handles, urd::concurrent>;
static_assert(std::same_as<StateBuilders::representation, urd::repr::SlotVector<std::vector<std::uint32_t>, true>>);

// tyr planning/{ground,lifted}/state_storage: TreeBitsetSet<uint_t, …, arity 4> for each state's fluent facts and
// TreeVectorSet<float_t, …, arity 4> for its numeric values: rows interned, equal subtrees shared across states.
using FluentFacts = Store<value<Store<key<Atom>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>;
static_assert(std::same_as<FluentFacts::representation, urd::repr::TreeBitsetSet<Store<key<Atom>, urd::dense, urd::frozen>>>);
using NumericValues = Store<value<Store<value<double>, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing>;
static_assert(std::same_as<NumericValues::representation, urd::repr::TreeVectorSet<Store<value<double>, urd::frozen>>>);
// The same storages with ThreadSafe, shared by threads that expand states at once.
using SharedFluentFacts = Store<value<Store<key<Atom>, urd::dense, urd::frozen>>, urd::handles, urd::unique, urd::structural_sharing, urd::concurrent>;
static_assert(std::same_as<SharedFluentFacts::representation, urd::repr::TreeBitsetSet<Store<key<Atom>, urd::dense, urd::frozen>, 4, true>>);

}  // namespace

// The maps further up are keyed by indices too; where their sites say so, they become dense requests.
