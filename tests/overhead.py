"""What a Store costs to compile beyond its container, per container: compiles tests/overhead/containers.cpp with
one element type and with --types of them, holding Stores and, with URD_OVERHEAD_DIRECT, the containers they choose
named directly. The difference between the two counts, over the containers it adds (three per type), is the cost of
one container without what the headers cost; the Store's minus the direct one's is urd's.

    python3 -I overhead.py --types N --limit-mb M -- <compiler command...>

Fails when a compile fails or urd's memory per container exceeds the limit. Time is reported, never limited, as in
tests/budget.py: it depends on the machine and on what else ctest runs in parallel.
"""

import argparse
import os
import subprocess
import sys
import time

KINDS = 3  # a set, a sequence and a map per element type


def cost(command: list[str], types: int, direct: bool) -> tuple[float, float]:
    """Seconds and peak MB of one compile: the process and the compiler it waits for, as the kernel accounts it."""
    defines = [f"-DURD_OVERHEAD_TYPES={types}", *(["-DURD_OVERHEAD_DIRECT"] if direct else [])]
    started = time.perf_counter()
    process = subprocess.Popen([*command, *defines])
    _, status, usage = os.wait4(process.pid, 0)
    elapsed = time.perf_counter() - started
    if os.waitstatus_to_exitcode(status) != 0:
        raise SystemExit(f"overhead: the compile failed ({'direct' if direct else 'Store'}, {types} types)")
    peak = usage.ru_maxrss / (1024 * 1024) if sys.platform == "darwin" else usage.ru_maxrss / 1024
    return elapsed, peak


def per_container(command: list[str], types: int, direct: bool) -> tuple[float, float]:
    """Milliseconds and MB one container adds: the compile with `types` element types minus the one with one."""
    one = cost(command, 1, direct)
    many = cost(command, types, direct)
    containers = KINDS * (types - 1)
    return (many[0] - one[0]) * 1000 / containers, (many[1] - one[1]) / containers


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--types", type=int, default=40, help="element types, each held in a set, a sequence and a map")
    parser.add_argument("--limit-mb", type=float, required=True, help="urd's memory per container")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="the compiler command, after --")
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if not command or arguments.types < 2:
        parser.error("a compiler command and at least two types are needed")

    store_ms, store_mb = per_container(command, arguments.types, direct=False)
    direct_ms, direct_mb = per_container(command, arguments.types, direct=True)
    urd_ms, urd_mb = store_ms - direct_ms, store_mb - direct_mb
    print(f"overhead per container: Store {store_ms:.1f} ms, {store_mb:.2f} MB; direct {direct_ms:.1f} ms, {direct_mb:.2f} MB")
    print(f"overhead per container: urd {urd_ms:.1f} ms, {urd_mb:.2f} MB (limit {arguments.limit_mb:.2f} MB)")
    if urd_mb > arguments.limit_mb:
        print("overhead: over the memory limit")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
