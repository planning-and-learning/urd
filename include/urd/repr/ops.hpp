#pragma once

#include <cstddef>
#include <vector>

namespace urd::repr
{

/// The common interface through which a Store uses a representation R, whatever R's own interface is. Each
/// representation specializes it:
///   static R make(std::vector<E>);          // from any elements; does the work the representation needs
///   static R assume(std::vector<E>);        // from elements claimed to have the Store's properties
///   static bool insert(R&, const E&);       // whether it was inserted
///   static std::size_t erase(R&, const E&); // how many were removed; only if R is shrinkable
///   static void clear(R&);                  // only if R is shrinkable
///   static bool contains(const R&, const E&);       // sets
///   static const E& at(const R&, std::size_t pos);  // sequences
///
/// What R guarantees is declared by container_traits<R> (traits.hpp), and every operation keeps it. The
/// exceptions are part of the properties' definitions: erase and clear end the references to what they
/// remove, and concurrent covers only inserts and reads.
template<typename R>
struct ops;

// A candidate registers a representation with Store, as a template over the spec S it is asked for:
//   using type = …;                          // the representation for S
//   static constexpr std::string_view name;  // for messages and the catalog
//   static constexpr bool accepts;           // whether it can represent S at all (kind, element types)
//   static constexpr int rank;               // for S's workload; lower is better

}  // namespace urd::repr
