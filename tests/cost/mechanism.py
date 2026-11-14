"""What urd's mechanism costs to compile, per Store, on the mix of Stores a downstream library holds.

The Stores are mechanism/stores.cpp's. The cost is split into its parts:

- container: the container a Store chose, named directly (the direct build);
- facets and Store class: what a Store adds to its container once its spec and container are known (resolved - direct);
- selection: turning the request into its spec and choosing its container (full - resolved).

    python3 -I mechanism.py [--records K] [--runs N] [--limit-mb PART=MB ...] -- <compiler command...>

The command compiles mechanism/stores.cpp, syntax only. A clang probe of the file (CLANG, default clang++) spells each
Store's request, spec and container; the resolved and direct builds read them from mechanism_resolved.hpp, written to a
temporary directory. Each build is compiled with 1 and with K record types; a part's cost per Store is the difference
of the two, over the Stores the K - 1 added record types hold, the least of N runs. Reports memory (peak MB) and time
(ms) per Store and part, and fails where a part's memory exceeds its limit.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STORES_PER_RECORD = 12  # the Stores mechanism/stores.cpp's Requests<I> holds per record type
PARTS = ("container", "facets and Store class", "selection")


def split_arguments(text: str) -> list[str]:
    """The top-level template arguments of `text`, the inside of a template's angle brackets."""
    arguments: list[str] = []
    depth, current = 0, ""
    for character in text:
        if character in "<(":
            depth += 1
        elif character in ">)":
            depth -= 1
        if character == "," and depth == 0:
            arguments.append(current.strip())
            current = ""
        else:
            current += character
    arguments.append(current.strip())
    return arguments


def probe(command: list[str], records: int, clang: str) -> list[tuple[str, str, str]]:
    """Each Store's request (its kind and tags), spec and container, as clang spells them."""
    probe_command = [
        clang,
        *command[1:],
        f"-DURD_MECHANISM_RECORDS={records}",
        "-DURD_MECHANISM_PROBE",
        "-Wno-unknown-warning-option",
    ]
    probe_command = [argument for argument in probe_command if not argument.startswith("-Werror")]
    result = subprocess.run(probe_command, capture_output=True, text=True, check=False)
    found: dict[tuple[int, int], tuple[str, str, str]] = {}
    for line in result.stderr.splitlines():
        match = re.search(r"warning: 'probe<(.*)>' is deprecated: urd mechanism probe", line)
        if match:
            record, position, request, spec, representation = split_arguments(match.group(1))
            found[(int(record), int(position))] = (request, spec, representation)
    if result.returncode != 0 or len(found) != records * STORES_PER_RECORD:
        sys.stderr.write(result.stderr[-4000:])
        msg = f"mechanism: the probe found {len(found)} Stores, not {records * STORES_PER_RECORD}"
        raise SystemExit(msg)
    return [found[key] for key in sorted(found)]


def resolved_header(stores: list[tuple[str, str, str]]) -> str:
    """mechanism_resolved.hpp: each Store's spec and container given (store_parts), its container by name (chosen)."""
    lines = ["#pragma once", "", "namespace mechanism", "{", "template<typename Store>", "struct chosen;", "}", ""]
    for request, spec, representation in stores:
        arguments = request.removeprefix("boost::mp11::mp_list<").removesuffix(">")
        lines += [
            f"template<> struct urd::detail::store_parts<{arguments}>",
            f"{{ using spec = {spec}; using representation = {representation}; }};",
            f"template<> struct mechanism::chosen<urd::Store<{arguments}>>",
            f"{{ using type = {representation}; }};",
        ]
    return "\n".join(lines) + "\n"


def compile_once(command: list[str], defines: list[str]) -> tuple[float, float]:
    """Seconds and peak MB of one compile, as the kernel accounts the compiler."""
    started = time.perf_counter()
    process = subprocess.Popen([*command, *defines])
    _, status, usage = os.wait4(process.pid, 0)
    elapsed = time.perf_counter() - started
    if os.waitstatus_to_exitcode(status) != 0:
        msg = f"mechanism: the compile failed ({' '.join(defines)})"
        raise SystemExit(msg)
    peak = usage.ru_maxrss / (1024 * 1024) if sys.platform == "darwin" else usage.ru_maxrss / 1024
    return elapsed, peak


def least(command: list[str], defines: list[str], runs: int) -> tuple[float, float]:
    """The least time and the least peak memory of `runs` compiles."""
    measured = [compile_once(command, defines) for _ in range(runs)]
    return min(each[0] for each in measured), min(each[1] for each in measured)


def per_store(command: list[str], mode: list[str], records: int, runs: int) -> tuple[float, float]:
    """Milliseconds and MB one Store adds in a build: the compile with `records` record types minus the one with one."""
    one = least(command, [*mode, "-DURD_MECHANISM_RECORDS=1"], runs)
    many = least(command, [*mode, f"-DURD_MECHANISM_RECORDS={records}"], runs)
    stores = STORES_PER_RECORD * (records - 1)
    return (many[0] - one[0]) * 1000 / stores, (many[1] - one[1]) / stores


def main() -> int:
    """Measure each part per Store; 1 where a part's memory exceeds its limit."""
    parser = argparse.ArgumentParser(description=(__doc__ or "").partition("\n\n")[0])
    parser.add_argument("--records", type=int, default=20, help="record types, each holding the Stores of one family")
    parser.add_argument("--runs", type=int, default=3, help="compiles per build; the least is taken")
    parser.add_argument("--clang", default=os.environ.get("CLANG", "clang++"), help="the compiler of the probe")
    parser.add_argument(
        "--limit-mb", action="append", default=[], metavar="PART=MB", help=f"a limit per Store, PART one of {PARTS}"
    )
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if not command or arguments.records < 2:
        parser.error("a compiler command and at least two record types are needed")
    limits = dict(each.split("=", 1) for each in arguments.limit_mb)

    with tempfile.TemporaryDirectory() as directory:
        Path(directory, "mechanism_resolved.hpp").write_text(
            resolved_header(probe(command, arguments.records, arguments.clang))
        )
        command = [command[0], f"-I{directory}", *command[1:]]
        direct = per_store(command, ["-DURD_MECHANISM_DIRECT"], arguments.records, arguments.runs)
        resolved = per_store(command, ["-DURD_MECHANISM_RESOLVED"], arguments.records, arguments.runs)
        full = per_store(command, [], arguments.records, arguments.runs)

    parts = {
        "container": direct,
        "facets and Store class": (resolved[0] - direct[0], resolved[1] - direct[1]),
        "selection": (full[0] - resolved[0], full[1] - resolved[1]),
    }
    sys.stdout.write(
        f"per Store, {arguments.records} record types x {STORES_PER_RECORD} Stores, least of {arguments.runs} runs:\n"
    )
    failed = False
    for part, (milliseconds, megabytes) in parts.items():
        limit = limits.get(part)
        over = limit is not None and megabytes > float(limit)
        failed = failed or over
        sys.stdout.write(
            f"  {part:24s} {milliseconds:7.2f} ms {megabytes:6.3f} MB"
            + (f" (limit {limit} MB{', over' if over else ''})" if limit else "")
            + "\n"
        )
    sys.stdout.write(f"  {'whole Store':24s} {full[0]:7.2f} ms {full[1]:6.3f} MB\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
