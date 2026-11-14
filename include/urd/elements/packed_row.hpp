#pragma once

// A row of a pool whose request promises urd::by_column_widths, as its readers see it: its columns decoded on access
// (get(i), get<T>(i)), a borrowed random-access range of their values; with ThreadSafe, read through relaxed atomics.
// What such a pool's at(h) and view(h) hand out. Declared here and defined in urd/repr/containers/schema_rows.hpp, which
// the pool's header includes: code that only names it (a return type, a parameter) includes this header, code that
// reads one the definition.

namespace urd::repr
{
template<bool ThreadSafe>
class SchemaRow;
}  // namespace urd::repr

namespace urd
{
/// A row of column widths, read: its public name (its definition is urd::repr's).
template<bool ThreadSafe>
using packed_row = repr::SchemaRow<ThreadSafe>;
}  // namespace urd
