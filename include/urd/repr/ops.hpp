#pragma once

#include <cstddef>
#include <vector>

namespace urd::repr
{

/// How a Store builds and changes a representation R. Each representation specializes it:
///   static R make(std::vector<E>);          // from any elements; does the work the representation needs
///   static R assume(std::vector<E>);        // from elements claimed to have the Store's properties
///   static bool insert(R&, const E&);       // whether it was inserted
///   static std::size_t erase(R&, const E&); // how many were removed
///   static bool contains(const R&, const E&);
template<typename R>
struct ops;

}  // namespace urd::repr
