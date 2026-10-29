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
#include <tuple>
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

// tyr datalog/fact_sets.hpp: FunctionFactSet, row -> interval in the order first set: a remap from rows to positions,
// plus the rows and the intervals by position (get_bindings, get_values); reset forgets all. Merging by hull is an
// operation on top (step 2).
struct Interval
{
    double lower = 0;
    double upper = 0;

    static constexpr auto identifying_members() noexcept { return urd::Members<&Interval::lower, &Interval::upper> {}; }
};
using FunctionFacts = Store<key<Row>, value<Interval>, urd::dense, urd::insertion_ordered>;
static_assert(std::same_as<FunctionFacts::representation, urd::repr::OrderedMap<urd::repr::SparseSet<Row, true>, Interval>>);

// tyr formalism/unification/substitution.hpp: SubstitutionFunction, a fixed list of parameters, in order, each bound to
// an object or not; equal when the same parameters in the same order have the same bindings.
using Substitution = Store<key<ParameterIndex>, value<std::optional<Object>>, urd::dense, urd::insertion_ordered>;
static_assert(std::same_as<Substitution::representation, urd::repr::OrderedMap<urd::repr::SparseSet<ParameterIndex, true>, std::optional<Object>>>);

// tyr datalog/policies/annotation.hpp: IndexedAchievers, achievers in a vector plus an UnorderedMap<RuleBinding, size_t>
// of their positions: a map from bindings to witnesses in insertion order.
struct Witness
{
    double cost = 0;
};
using IndexedAchievers = Store<key<RelationBinding>, value<Witness>, urd::insertion_ordered>;
static_assert(std::same_as<IndexedAchievers::representation, urd::repr::OrderedMap<urd::repr::IndexedSet<RelationBinding>, Witness>>);

// tyr datalog/policies/annotation_types.hpp: DenseRelationMap<R, Value, ThreadSafe>, (relation, row) -> value,
// cleared by generation, update(key, f(value, initialized)) reusing stale values, ThreadSafe with a lock per slot:
// a dense map per relation of dense maps by row. Clearing all costs one clear per relation.
using RowAnnotations = Store<key<Row>, value<double>, urd::dense, urd::constant_clear>;
using RelationAnnotations = Store<key<Predicate>, value<RowAnnotations>, urd::dense>;
static_assert(std::same_as<RowAnnotations::representation, urd::repr::IndexMap<Row, double, true>>);
static_assert(std::same_as<RelationAnnotations::representation, urd::repr::IndexMap<Predicate, RowAnnotations, false>>);
using SharedRowAnnotations = Store<key<Row>, value<double>, urd::dense, urd::constant_clear, urd::concurrent>;
static_assert(std::same_as<SharedRowAnnotations::representation, urd::repr::IndexMap<Row, double, true, true>>);

// tyr's NumericIntervalAnnotations: per (relation, row) a few annotations, one per interval, kept cheapest first by
// (cost, lower, upper); insert-if-cheaper is merge, find(interval) a lookup by key, find(binding) the last entry; the
// rows cleared by generation, a lock per slot with ThreadSafe. Identity (the interval) and order (the cost) differ:
// order_by<P> with P(key, value).
struct ClosedInterval
{
    double lower = 0;
    double upper = 0;
    static constexpr auto identifying_members() noexcept { return urd::Members<&ClosedInterval::lower, &ClosedInterval::upper> {}; }
};
struct IntervalAnnotation
{
    double cost = 0;
    std::uint32_t witness = 0;
};
constexpr auto cheapest_interval_first = [](const ClosedInterval& interval, const IntervalAnnotation& annotation)
{ return std::tuple(annotation.cost, interval.lower, interval.upper); };
using IntervalAnnotations = Store<key<ClosedInterval>, value<IntervalAnnotation>, urd::sorted, urd::small, urd::order_by<cheapest_interval_first>>;
using NumericIntervalAnnotations = Store<key<Predicate>, value<Store<key<Row>, value<IntervalAnnotations>, urd::dense, urd::constant_clear>>, urd::dense>;
static_assert(
    std::same_as<IntervalAnnotations::representation,
                 urd::repr::FlatEntryMap<ClosedInterval, IntervalAnnotation, urd::EntryLess<ClosedInterval, IntervalAnnotation, cheapest_interval_first>>>);
static_assert(std::same_as<NumericIntervalAnnotations::representation::mapped_type::representation, urd::repr::IndexMap<Row, IntervalAnnotations, true>>);

// tyr's DeduplicatedAdjacencyMatrix (specific to k-partite graphs, so no urd container of its own) as a composition
// of general pools: each distinct partition bitset once (a pool of dense-set rows, read back as a BitsetSpan by
// view), each distinct vertex row once (a pool of rows of those handles), a row handle per vertex.
using PartitionBitset = Store<key<std::uint32_t>, urd::dense>;
using PartitionBitsets = Store<value<PartitionBitset>, urd::handles, urd::unique>;
using DistinctVertexRows = Store<value<Store<value<PartitionBitsets::handle_type>>>, urd::handles, urd::unique>;
using VertexRows = Store<value<DistinctVertexRows::handle_type>>;
static_assert(std::same_as<PartitionBitsets::representation, urd::repr::RowInterner<PartitionBitset>>);
static_assert(std::same_as<decltype(PartitionBitsets().view(PartitionBitsets::handle_type {})), urd::repr::BitsetSpan<const std::uint64_t>>);

// The second pass (docs/SURVEY.md, 2026-10-29), on the libraries' newest branches: what the first pass left as a gap
// and later milestones closed, pinned from the sites that asked for it.

// yggdrasil database/search/a_star.hpp: OpenList, now ordered by f ascending, then larger g, then the later push: a
// priority queue by a projection (M10.2), its storage kept across searches.
struct AStarEntry
{
    double f = 0;
    double g = 0;
    std::uint64_t push = 0;
    std::uint32_t vertex = 0;
};
constexpr auto a_star_order = [](const AStarEntry& entry) { return std::tuple(entry.f, -entry.g, -static_cast<std::int64_t>(entry.push)); };
using AStarOpenList = Store<value<AStarEntry>, urd::by_priority, urd::retains, urd::order_by<a_star_order>>;
static_assert(
    std::same_as<AStarOpenList::representation, urd::repr::Queue<AStarEntry, urd::repr::leaving::least, urd::ProjectedLess<AStarEntry, a_star_order>>>);

// yggdrasil datalog/detail/strata.hpp, WeightedStratum: a heap of candidates by weight with lazy deletion (stale
// entries skipped on pop); also tyr's ground scheduler and tree repair. With handles, an entry is updated or erased
// in place instead.
struct Candidate
{
    std::uint32_t row = 0;
    std::uint64_t priority = 0;
};
using Candidates = Store<value<Candidate>, urd::by_priority, urd::handles, urd::order_by<&Candidate::priority>>;
static_assert(std::same_as<Candidates::representation, urd::repr::AddressableHeap<Candidate, urd::ProjectedLess<Candidate, &Candidate::priority>>>);

// tyr datalog/lifted/scheduler.hpp, TypedRuleSchedulerStratum: the active rules as a hash set, copied into a vector
// and sorted every iteration for a stable order. A bitset iterates its indices ascending by itself.
using ActiveRules = Store<key<Rule>, urd::dense, urd::sorted>;
static_assert(std::same_as<ActiveRules::representation, urd::repr::DenseSet<Rule>>);

// tyr datalog/ground/workspaces/program.hpp, GroundRuleDependencies: per relation, the rules to fire again for each
// row, built once and never cleared: a frozen multimap per relation, in a frozen dense map.
using RowRules = Store<key<Row>, value<Rule>, urd::multi, urd::dense, urd::frozen>;
using RuleDependencies = Store<key<Predicate>, value<RowRules>, urd::dense, urd::frozen>;
static_assert(std::same_as<RowRules::representation, urd::repr::CsrMultiMap<Row, Rule>>);
static_assert(std::same_as<RuleDependencies::representation, urd::repr::IndexMap<Predicate, RowRules, false>>);

// tyr analysis/static_literal_compatibility.hpp, kckp.hpp (three strided accessors), runir weisfeiler_leman.hpp and
// icp/dl/parser.cpp (Warshall closure): bit matrices as rows of a fixed length over one word array.
using BitMatrix = Store<value<Store<key<Vertex>, urd::dense>>, urd::fixed_shape>;
static_assert(std::same_as<BitMatrix::representation, urd::repr::Jagged<Store<key<Vertex>, urd::dense>>>);

// runir datasets/equivalence_policies/gi.hpp: an UnorderedMap<SparseGraph, VertexRef> under a mutex, found or
// allocated by every worker: a concurrent map, the allocation in update under the shard's lock.
struct CanonicalGraphTag;
using CanonicalGraph = Id<CanonicalGraphTag>;
#if defined(URD_HAS_GTL)
using Representatives = Store<key<CanonicalGraph>, value<Vertex>, urd::concurrent>;
static_assert(std::same_as<Representatives::representation, urd::repr::ParallelHashMap<CanonicalGraph, Vertex>>);
#endif

// runir kr/parser/ext_constructors.hpp, ReferenceTable: a name -> entity map beside a vector of the entities by
// identifier, six times over; loki builder_declarations.cpp: declarations beside their declaration order.
using References = Store<key<std::string>, value<Predicate>, urd::insertion_ordered>;
static_assert(std::same_as<References::representation, urd::repr::OrderedMap<urd::repr::IndexedSet<std::string>, Predicate>>);

// runir datasets/object_graph_color.hpp, ColorRepository: a concurrent symbol repository of three record types.
template<typename T>
using SharedInterned = Store<key<T>, urd::positional, urd::monotone, urd::concurrent>;
using Colors = urd::compose::Family<SharedInterned, Predicate, Object, Vertex>;
static_assert(std::same_as<SharedInterned<Object>::representation, urd::repr::ConcurrentIndexedSet<Object>>);
static_assert(std::same_as<decltype(std::declval<Colors&>().get<Object>()), SharedInterned<Object>&>);

// runir kr/dl/semantics/evaluation_workspace.hpp and yggdrasil evaluation/bound.hpp: std::deque chosen so that
// references stay valid while appending.
struct KeyIndex
{
    std::vector<std::uint32_t> positions;
};
using KeyIndexes = Store<value<KeyIndex>, urd::monotone, urd::stable_refs>;
static_assert(std::same_as<KeyIndexes::representation, urd::repr::SegmentedVector<KeyIndex>>);

// yggdrasil evaluation/incremental_bound.hpp (plans keyed by a vector<bool> of bound variables), tyr seen_bindings
// (keyed by an IndexList<Object>), loki union types (keyed by a sorted name list): maps keyed by a Store. A Store
// whose contents fix its order (sorted here: a bitset in key order) is hashable, so a hashed map takes it; a sorted
// Store is orderable too, so a sorted map takes it (below).
using SeedPlan = Store<key<std::uint32_t>, urd::dense, urd::frozen, urd::sorted>;
using SeedPlans = Store<key<SeedPlan>, value<std::uint32_t>>;
static_assert(std::same_as<SeedPlan::representation, urd::repr::DenseSet<std::uint32_t>>
              && std::same_as<SeedPlans::representation, FlatHashMapOf<SeedPlan, std::uint32_t>>);

// yggdrasil operators/distance/incremental/distance.hpp: the active sources, compacted by swapping the last one into
// the erased slot, with a root -> position map: a dense set whose elements lie next to each other.
using ActiveSources = Store<key<Vertex>, urd::dense, urd::contiguous>;
static_assert(std::same_as<ActiveSources::representation, urd::repr::SparseSet<Vertex>>);

// loki src/semantic/context.hpp: union types keyed by their sorted member names (std::map<vector<string>, string>),
// and the type table collected in index order at build time, with M11 an order the map keeps itself.
using TypeNames = Store<key<std::string>, urd::sorted, urd::frozen>;
using UnionTypes = Store<key<TypeNames>, value<std::string>, urd::sorted>;
static_assert(std::same_as<UnionTypes::representation, TreeMapOf<TypeNames, std::string>>);
struct TypeTag;
using Type = Id<TypeTag>;
constexpr auto by_type_index = [](const std::string&, const Type& type) { return type.value; };
using TypesByIndex = Store<key<std::string>, value<Type>, urd::sorted, urd::small, urd::order_by<by_type_index>>;
static_assert(std::same_as<TypesByIndex::representation, urd::repr::FlatEntryMap<std::string, Type, urd::EntryLess<std::string, Type, by_type_index>>>);
}  // namespace

// The maps further up are keyed by indices too; where their sites say so, they become dense requests.
