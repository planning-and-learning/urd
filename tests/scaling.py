"""Whether a file holding a Store pays only for its own container: compiles it with Clang's -ftime-trace, with its
Store and without it (URD_SCALING_BASELINE), and fails when the Store adds an instantiation of any registered container
type other than the chosen one. The baseline takes out what the headers instantiate themselves (the standard library's
<functional> instantiates std::unordered_map<int, int>).

    python3 -I scaling.py --clang <clang++> --allow <type prefix>... -- <compiler command...>

The compiler command is the one CMake gives for the file (tests/CMakeLists.txt, urd_syntax_only); its compiler is
replaced by Clang and it is run to an object, with a trace of every instantiation. A selection that walked the
registry's containers, or a header that instantiated them, shows here as instantiations of containers no Store chose.
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

# What a registered container's type begins with: urd's own, and the standard library's, Boost's and gtl's.
URD = ("Jagged", "RowPool", "IndexedSet", "Queue", "AddressableHeap", "BucketQueue", "RingFifo", "DenseSet", "SparseSet",
       "IndexMap", "GroupedMultiMap", "BidirectionalCsr", "DisjointSets", "Layered", "LayeredPool", "ChangeLogged",
       "OrderedMap", "SegmentedVector", "RecordInterner", "ArenaInterner", "RowInterner", "TreeVectorSet", "TreeBitsetSet",
       "SlotVector", "Scoped", "Recycling", "RecyclingSet", "FlatEntryMap")
CONTAINERS = (*(f"urd::repr::{each}<" for each in URD), "std::set<", "std::multiset<", "std::map<", "std::multimap<",
              "std::unordered_set<", "std::unordered_map<", "std::unordered_multimap<",
              "boost::container::flat_set<", "boost::container::flat_multiset<", "boost::container::flat_map<",
              "boost::container::flat_multimap<", "boost::unordered::unordered_flat_set<",
              "boost::unordered::unordered_flat_map<", "gtl::parallel_")


def instantiated(command: list[str], clang: str, *extra: str) -> set[str]:
    """The classes the command instantiates, by Clang's trace."""
    with tempfile.TemporaryDirectory() as scratch:
        output = Path(scratch) / "scaling.o"
        run = [clang, *[each for each in command[1:] if each != "-fsyntax-only"], "-c", "-o", str(output),
               "-ftime-trace", "-ftime-trace-granularity=0", *extra]
        done = subprocess.run(run, check=False, capture_output=True, text=True)
        if done.returncode != 0:
            sys.stderr.write(done.stderr[-4000:])
            raise SystemExit(f"the command failed with status {done.returncode}")
        trace = json.loads(output.with_suffix(".json").read_text())
    return {event["args"]["detail"] for event in trace["traceEvents"]
            if event.get("name") == "InstantiateClass" and "detail" in event.get("args", {})}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--allow", action="append", default=[], help="a chosen container's type prefix")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="the compiler command, after --")
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    baseline = instantiated(command, arguments.clang, "-DURD_SCALING_BASELINE")
    added = instantiated(command, arguments.clang) - baseline
    containers = sorted(each for each in added if each.startswith(CONTAINERS))
    others = [each for each in containers if not each.startswith(tuple(arguments.allow))]
    print(f"scaling: the Store adds {len(containers)} container instantiations, {len(others)} of containers it did not choose")
    for each in others:
        print(f"  not chosen, but instantiated: {each}")
    return 1 if others else 0


if __name__ == "__main__":
    sys.exit(main())
