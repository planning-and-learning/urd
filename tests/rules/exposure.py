# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later
"""What urd exposes publicly, checked with clang's own parser (libclang, from PyPI).

Two rules (AGENTS.md):

- F7, only facets expose: a container (a class urd::detail::declaration_of names) has no public member function, its
  bases' included, that is neither forwarded by a facet of its kind (`representation().name(...)`,
  `store_access::of<Derived>(*this).name(...)`) nor a member of its kind's counterpart in the standard library (rule I1:
  std::vector for a sequence, std::set or std::map for a set or map, std::queue for a queue). Its declaration names its
  kind, its kind its facets (urd/kinds.hpp), every kind the common ones.
  What urd's own code needs beyond the facets goes through a friend of the container: an accessor in urd::detail, or the
  one component that uses it. Construction, assignment, comparison and the protocols urd reads of a type
  (memory_members, identifying_members) are what every container has. A container inherits publicly from nothing
  outside urd (a standard or third-party table, or a template parameter), whose whole interface it would expose.
- F8, no representation in a facet's signature: a facet's public member neither takes nor returns words or bitsets
  (word_t, bitset_span, BitsetSpan).

    python3 tests/rules/exposure.py --compile-commands build/compile_commands.json

parses every container's header (tests/support/all_containers.hpp) and the Store with the flags the build compiles its tests
with, writes each finding, and exits 1 where a rule has more findings than it has open, or fewer (its count is
then lowered).
"""

import argparse
import json
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict
from collections.abc import Iterable, Iterator
from pathlib import Path

import clang.cindex as ci

ROOT = Path(__file__).resolve().parents[2]
STORE = ROOT / "include" / "urd" / "store"

# What every container has, which no facet need forward: construction, assignment, comparison, and the protocols urd
# reads of a type rather than calling through a facet: memory_members (urd::memory_of, rule D3) and identifying_members
# (urd::EqualTo, Hash and Less).
EVERY_CONTAINER = {
    "memory_members",
    "identifying_members",
    "operator=",
    "operator==",
    "operator!=",
    "operator<=>",
    "operator<",
}
# Per kind, the members of its counterpart in the standard library, which its containers have as theirs (rule I1): a
# container is also what other containers hold as storage (a segmented vector holds an indexed set's elements), through
# that interface. A kind without one (a pool, slots, a partition, a graph) has only its facets.
CONTAINER = frozenset(
    {"begin", "end", "cbegin", "cend", "rbegin", "rend", "crbegin", "crend", "empty", "size", "max_size"}
)
SEQUENCE = CONTAINER | {
    *(
        "at",
        "operator[]",
        "front",
        "back",
        "data",
        "reserve",
        "capacity",
        "shrink_to_fit",
        "clear",
        "insert",
        "emplace",
    ),
    *("erase", "push_back", "emplace_back", "pop_back", "resize", "swap", "assign"),
}
ASSOCIATIVE = CONTAINER | {
    *("clear", "insert", "emplace", "emplace_hint", "erase", "swap", "extract", "merge", "count", "find", "contains"),
    *("equal_range", "lower_bound", "upper_bound", "key_comp", "value_comp", "reserve", "rehash", "bucket_count"),
    *("load_factor", "max_load_factor", "hash_function", "key_eq"),
}
QUEUE = {"front", "back", "top", "push", "emplace", "pop", "empty", "size", "swap"}
STANDARD_COUNTERPART = {
    "sequence_kind": SEQUENCE,
    "set_kind": ASSOCIATIVE,
    "multiset_kind": ASSOCIATIVE,
    "map_kind": ASSOCIATIVE | {"at", "operator[]", "insert_or_assign", "try_emplace"},
    "queue_kind": QUEUE,
}
# Public bases outside urd that a container may have, each with why: a template parameter that is always one of urd's
# containers, checked itself.
URD_CONTAINER_BASES = {
    (
        "urd::repr::Numbered",
        "R",
    ): "Numbered<R> is urd's container R with the number of its Store: R's interface is R's own",
}
# The findings each rule has today (AGENTS.md: an open rule's count must not grow): F8 until tyr's state storage stops
# reading a dense set's words.
OPEN = {"F7": 0, "F8": 3}
# F8: what a facet's signature must not name.
REPRESENTATION_WORDS = ("word_t", "bitset_span", "BitsetSpan", "word_bits")
METHODS = {ci.CursorKind.CXX_METHOD, ci.CursorKind.FUNCTION_TEMPLATE, ci.CursorKind.CONVERSION_FUNCTION}
CLASSES = {
    ci.CursorKind.CLASS_DECL,
    ci.CursorKind.STRUCT_DECL,
    ci.CursorKind.CLASS_TEMPLATE,
    ci.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION,
}
# The operators a facet applies to its container, by the token that follows the call.
OPERATOR_TOKENS = {
    "[": "operator[]",
    "|=": "operator|=",
    "&=": "operator&=",
    "-=": "operator-=",
    "==": "operator==",
    "=": "operator=",
}
# The standard functions a facet applies to its container, and the member each reads.
STANDARD_READS = {
    "cdata": "data",
    "data": "data",
    "begin": "begin",
    "end": "end",
    "cbegin": "cbegin",
    "cend": "cend",
    "size": "size",
}
# The parse errors written before giving up.
ERRORS_WRITTEN = 10

Classes = dict[str, ci.Cursor]
Places = list[tuple[str, str]]


def place(cursor: ci.Cursor) -> str:
    """Where `cursor` is declared: its file and line."""
    file = cursor.location.file
    return f"{file.name if file is not None else '?'}:{cursor.location.line}"


def say(line: str) -> None:
    """Writes one line of the report."""
    sys.stdout.write(line + "\n")


def qualified(cursor: ci.Cursor | None) -> str:
    """The name of `cursor` with its namespaces and classes (urd::repr::DenseSet)."""
    parts: list[str] = []
    while cursor is not None and cursor.kind != ci.CursorKind.TRANSLATION_UNIT:
        if cursor.spelling:
            parts.append(cursor.spelling)
        cursor = cursor.semantic_parent
    return "::".join(reversed(parts))


def compile_flags(commands: str) -> list[str]:
    """The flags the build compiles a Store test with: includes, definitions, the standard, and clang's own headers.

    Args:
        commands: The build's compile_commands.json.

    Returns:
        The flags, for libclang.
    """
    entries: list[dict[str, str]] = json.loads(Path(commands).read_text())
    entry = next(each for each in entries if each["file"].endswith("tests/unit/store/graphs.cpp"))
    flags: list[str] = []
    skip = False
    for word in shlex.split(entry["command"])[1:]:
        if skip:
            skip = False
            continue
        if word in {"-o", "-c"}:
            skip = True
            continue
        if word.startswith(("-I", "-isystem", "-D", "-std")) or (flags and flags[-1] == "-isystem"):
            flags.append(word)
    # The PyPI wheel carries the parser, not clang's own headers (stddef.h, …): those of the clang installed.
    clang = shutil.which("clang++") or shutil.which("clang")
    if clang is None:
        sys.exit("exposure.py: needs clang++ for clang's own headers (clang++ -print-resource-dir)")
    resource = subprocess.run([clang, "-print-resource-dir"], capture_output=True, text=True, check=True).stdout.strip()
    return [*flags, "-resource-dir", resource, "-I" + str(ROOT / "tests"), "-x", "c++"]


def parse(flags: list[str]) -> ci.TranslationUnit:
    """Every container's header and the Store, parsed; a parse error ends the check (exit 2)."""
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as out:
        out.write('#include "all_containers.hpp"\n#include "urd/store.hpp"\n')
        name = out.name
    unit = ci.Index.create().parse(name, args=flags)
    errors = [each for each in unit.diagnostics if each.severity >= ci.Diagnostic.Error]
    if errors:
        for each in errors[:ERRORS_WRITTEN]:
            sys.stderr.write(f"exposure.py: parse error: {each}\n")
        sys.exit(2)
    return unit


def walk(cursor: ci.Cursor) -> Iterator[ci.Cursor]:
    """Every cursor under `cursor`, itself first."""
    yield cursor
    for child in cursor.get_children():
        yield from walk(child)


def containers_of(unit: ci.TranslationUnit) -> set[str]:
    """The containers: the classes urd::detail::declaration_of is specialized for (declaration_of<repr::X<…>>)."""
    names: set[str] = set()
    for cursor in walk(unit.cursor):
        if (
            cursor.spelling != "declaration_of"
            or cursor.kind == ci.CursorKind.CLASS_TEMPLATE
            or cursor.kind not in CLASSES
        ):
            continue
        if qualified(cursor.semantic_parent) != "urd::detail":
            continue
        # The first class of urd::repr its argument names (a wrapper's, not its part's).
        for child in cursor.get_children():
            if child.kind in {ci.CursorKind.TEMPLATE_REF, ci.CursorKind.TYPE_REF} and qualified(
                child.referenced
            ).startswith("urd::repr::"):
                names.add(qualified(child.referenced))
                break
    return names


def definitions(unit: ci.TranslationUnit) -> Classes:
    """Every class definition in urd, by qualified name (a template's primary definition)."""
    found: Classes = {}
    for cursor in walk(unit.cursor):
        if cursor.kind in CLASSES and cursor.is_definition() and qualified(cursor).startswith("urd::"):
            found.setdefault(qualified(cursor), cursor)
    return found


def public_methods(cursor: ci.Cursor | None, classes: Classes, seen: set[str] | None = None) -> dict[str, str]:
    """Its public member functions, and its urd bases', by name, with where each is declared."""
    seen = set() if seen is None else seen
    if cursor is None or qualified(cursor) in seen:
        return {}
    seen.add(qualified(cursor))
    result: dict[str, str] = {}
    for child in cursor.get_children():
        if child.kind == ci.CursorKind.CXX_BASE_SPECIFIER and child.referenced is not None:
            base = classes.get(qualified(child.referenced), child.referenced)
            if qualified(base).startswith("urd::"):
                result.update(public_methods(base, classes, seen))
    for child in cursor.get_children():
        # A constructor template is no member function to forward (its spelling is the class's).
        if (
            child.kind in METHODS
            and child.access_specifier == ci.AccessSpecifier.PUBLIC
            and child.spelling.split("<")[0] != cursor.spelling
        ):
            result[child.spelling] = place(child)
    return result


def outside_bases(cursor: ci.Cursor | None, classes: Classes, seen: set[str] | None = None) -> list[str]:
    """Its public bases, and its urd bases', that are not urd's: a standard or third-party type, or a parameter."""
    seen = set() if seen is None else seen
    if cursor is None or qualified(cursor) in seen:
        return []
    seen.add(qualified(cursor))
    result: list[str] = []
    for child in cursor.get_children():
        if child.kind != ci.CursorKind.CXX_BASE_SPECIFIER or child.access_specifier != ci.AccessSpecifier.PUBLIC:
            continue
        name = qualified(child.referenced) if child.referenced is not None else ""
        if name.startswith("urd::"):
            result += outside_bases(classes.get(name, child.referenced), classes, seen)
        else:
            result.append(child.type.spelling)
    return result


def nesting(tokens: list[str], positions: Iterable[int], opening: str, closing: str) -> Iterator[tuple[int, int]]:
    """Each position of a traversal of `tokens`, with how deep it is: one deeper at each `opening`, one less at each
    `closing` (so a backward traversal opens at `)`)."""
    depth = 0
    for at in positions:
        depth += tokens[at] == opening
        depth -= tokens[at] == closing
        yield at, depth


def callee_before(tokens: list[str], close: int) -> tuple[str, int]:
    """The name called by the call whose `)` is at `close`, and where its `(` is."""
    backward = range(close, -1, -1)
    at = next((at for at, depth in nesting(tokens, backward, ")", "(") if depth == 0), -1)
    return (tokens[at - 1] if at > 0 else ""), at


def next_close(tokens: list[str], at: int) -> int:
    """The `)` that closes the call `tokens[at]` sits in, else `at`."""
    forward = range(at + 1, len(tokens))
    return next((later for later, depth in nesting(tokens, forward, "(", ")") if depth < 0), at)


def reaches_container(tokens: list[str], close: int) -> bool:
    """Whether the call ending at `close` is `representation(...)` or `of<Derived>(*this)`: the facet's container."""
    callee, opening = callee_before(tokens, close)
    calls_representation = callee == "representation"
    # store_access::of<Derived>(*this): the call's `(` follows the `>` of the template `of`.
    calls_store_access = callee == ">" and template_before(tokens, opening - 1) == "of"
    return calls_representation or calls_store_access


def template_before(tokens: list[str], close: int) -> str:
    """The template whose argument list's `>` is at `close` (`of` in `of<Derived>`)."""
    backward = range(close, -1, -1)
    angle = next((at for at, depth in nesting(tokens, backward, ">", "<") if depth == 0), -1)
    return tokens[angle - 1] if angle > 0 else ""


def forwarded_in(tokens: list[str]) -> set[str]:
    """What the tokens of a facet call on its container.

    A member after `representation(...)` or `store_access::of<Derived>(*this)`, an operator applied to it, or a standard
    function that reads it (std::ranges::cdata(representation())).

    Args:
        tokens: The facet's tokens.

    Returns:
        The names of the members forwarded.
    """
    names: set[str] = set()
    for at, spelled in enumerate(tokens):
        if spelled != ")" or at + 1 >= len(tokens) or not reaches_container(tokens, at):
            continue
        following = tokens[at + 1]
        if following in {".", "->"} and at + 2 < len(tokens):
            names.add(tokens[at + 2])
        elif following in OPERATOR_TOKENS:
            names.add(OPERATOR_TOKENS[following])
        # std::ranges::cdata(representation()): the call whose argument this is.
        outer, _ = callee_before(tokens, next_close(tokens, at))
        if outer in STANDARD_READS:
            names.add(STANDARD_READS[outer])
    return names


def facet_names(unit: ci.TranslationUnit) -> dict[str, set[str]]:
    """Per facet (urd::detail::facet::X), what it forwards to its container; under "Store", what the Store does."""
    names: dict[str, set[str]] = defaultdict(set)
    bases: dict[str, set[str]] = defaultdict(set)
    for cursor in walk(unit.cursor):
        if cursor.kind not in CLASSES or not cursor.is_definition():
            continue
        if qualified(cursor.semantic_parent) == "urd::detail::facet":
            names[cursor.spelling] |= forwarded_in([token.spelling for token in cursor.get_tokens()])
            # A facet built on another (visiting on iteration) forwards what that one does.
            for child in cursor.get_children():
                if child.kind == ci.CursorKind.CXX_BASE_SPECIFIER and qualified(child.referenced).startswith(
                    "urd::detail::facet::"
                ):
                    bases[cursor.spelling].add(qualified(child.referenced).split("::")[-1])
        elif qualified(cursor) == "urd::Store":
            names["Store"] |= forwarded_in([token.spelling for token in cursor.get_tokens()])
    for facet in list(names):
        for base in bases[facet]:
            names[facet] |= names[base]
    return names


def alias_of(cursor: ci.Cursor, name: str) -> str | None:
    """What `using name = …;` in `cursor` names, as spelled."""
    for child in cursor.get_children():
        if child.kind == ci.CursorKind.TYPE_ALIAS_DECL and child.spelling == name:
            return child.underlying_typedef_type.spelling
    return None


def kind_facets(unit: ci.TranslationUnit) -> dict[str, set[str]]:
    """Per kind (set_kind, graph_kind, …), its facets: its own (facets) and every kind's (any_kind::common_facets)."""
    own: dict[str, set[str]] = {}
    common: set[str] = set()
    for cursor in walk(unit.cursor):
        if cursor.kind not in CLASSES or not cursor.is_definition() or not cursor.spelling.endswith("_kind"):
            continue
        listed = alias_of(cursor, "facets")
        if listed is not None:
            own[cursor.spelling] = set(re.findall(r"facet::(\w+)", listed))
        shared = alias_of(cursor, "common_facets")
        if shared is not None:
            common = set(re.findall(r"facet::(\w+)", shared))
    return {kind: facets | common for kind, facets in own.items()}


def kind_of_declaration(name: str, classes: Classes) -> str | None:
    """The kind declaration `name` (urd::detail::X_decl) states, its bases' where it inherits it; else none."""
    cursor = classes.get("urd::detail::" + name.partition("<")[0].rpartition("::")[2])
    while cursor is not None:
        kind = alias_of(cursor, "kind")
        if kind is not None:
            return kind.split("::")[-1]
        bases = [child.referenced for child in cursor.get_children() if child.kind == ci.CursorKind.CXX_BASE_SPECIFIER]
        cursor = classes.get(qualified(bases[0]), bases[0]) if bases and bases[0] is not None else None
    return None


def container_kinds(unit: ci.TranslationUnit, classes: Classes) -> dict[str, str | None]:
    """Per container, the kind its declaration states.

    None where its declaration_of names no declaration of its own: a wrapper over any container, as Numbered.

    Returns:
        The kind of each container, by name.
    """
    kinds: dict[str, str | None] = {}
    for cursor in walk(unit.cursor):
        if (
            cursor.spelling != "declaration_of"
            or cursor.kind == ci.CursorKind.CLASS_TEMPLATE
            or cursor.kind not in CLASSES
        ):
            continue
        if qualified(cursor.semantic_parent) != "urd::detail":
            continue
        container = next(
            (
                qualified(child.referenced)
                for child in cursor.get_children()
                if child.kind in {ci.CursorKind.TEMPLATE_REF, ci.CursorKind.TYPE_REF}
                and qualified(child.referenced).startswith("urd::repr::")
            ),
            None,
        )
        if container is None:
            continue
        declaration = alias_of(cursor, "declaration")
        kind = kind_of_declaration(declaration, classes) if declaration is not None else None
        kinds[container] = kind if kinds.get(container, kind) == kind else None
    return kinds


def facet_signatures(unit: ci.TranslationUnit) -> list[tuple[str, str, str, list[str]]]:
    """F8: the public members of the facets whose declarations name words or bitsets."""
    findings: list[tuple[str, str, str, list[str]]] = []
    for cursor in walk(unit.cursor):
        if (
            cursor.kind not in CLASSES
            or not cursor.is_definition()
            or qualified(cursor.semantic_parent) != "urd::detail::facet"
        ):
            continue
        for child in cursor.get_children():
            if child.kind not in METHODS or child.access_specifier != ci.AccessSpecifier.PUBLIC:
                continue
            declaration: list[str] = []
            for token in child.get_tokens():
                if token.spelling in {"{", ";"}:
                    break
                declaration.append(token.spelling)
            named = [word for word in REPRESENTATION_WORDS if word in declaration]
            if named:
                findings.append((place(child), cursor.spelling, child.spelling, named))
    return findings


def relative(where: str) -> str:
    """`file:line` with the file relative to urd; a place outside it as it is."""
    file, _, line = where.rpartition(":")
    return f"{Path(file).relative_to(ROOT)}:{line}" if file.startswith(str(ROOT)) else where


def exposed_members(unit: ci.TranslationUnit) -> tuple[dict[str, Places], int, list[str]]:
    """F7: per container, its public members no facet of its kind forwards to and its public bases outside urd.

    Returns:
        The findings by container, how many containers there are, and those checked against every facet's names, whose
        kind their declaration does not state.
    """
    classes = definitions(unit)
    containers = sorted(containers_of(unit))
    by_facet = facet_names(unit)
    facets_of = kind_facets(unit)
    kinds = container_kinds(unit, classes)
    every: set[str] = set[str]().union(*by_facet.values(), *STANDARD_COUNTERPART.values())
    exposed: dict[str, Places] = defaultdict(list)
    unknown: list[str] = []
    for name in containers:
        kind = kinds.get(name)
        if kind is None or kind not in facets_of:
            unknown.append(name)
            allowed = every
        else:
            allowed: set[str] = by_facet["Store"].union(
                *(by_facet[facet] for facet in facets_of[kind]), STANDARD_COUNTERPART.get(kind, set())
            )
        for method, where in sorted(public_methods(classes.get(name), classes).items()):
            if method not in allowed and method not in EVERY_CONTAINER:
                exposed[name].append((method, relative(where)))
        for base in outside_bases(classes.get(name), classes):
            if (name, base) not in URD_CONTAINER_BASES:
                exposed[name].append((f"public base {base}", ""))
    return exposed, len(containers), unknown


def main() -> int:
    """Writes the findings of F7 and F8; 1 where there is one."""
    parser = argparse.ArgumentParser(description="What urd exposes publicly, checked with clang's own parser.")
    parser.add_argument("--compile-commands", required=True)
    commands: str = parser.parse_args().compile_commands

    unit = parse(compile_flags(commands))
    exposed, containers, unknown = exposed_members(unit)
    count = sum(len(each) for each in exposed.values())
    say(f"F7 only facets expose: {count} public members of {len(exposed)} of {containers} containers no facet forwards")
    if unknown:
        say(f"  checked against every facet's names, their kind not stated: {', '.join(unknown)}")
    for name, members in sorted(exposed.items()):
        say(f"  {name}")
        for member, where in members:
            say(f"    {member:28} {where}")
    signatures = facet_signatures(unit)
    say(f"F8 no representation in a facet's signature: {len(signatures)}")
    for where, facet, method, named in signatures:
        say(f"  {facet}::{method} ({', '.join(named)})  {relative(where)}")
    return verdict({"F7": count, "F8": len(signatures)})


def verdict(found: dict[str, int]) -> int:
    """1 where a rule has more findings than its open count; where it has fewer, the count to lower it to."""
    failed = False
    for rule, count in found.items():
        if count > OPEN[rule]:
            say(f"{rule}: {count} findings, {OPEN[rule]} open: fix the new ones")
            failed = True
        elif count < OPEN[rule]:
            say(f"{rule}: {count} findings, {OPEN[rule]} open: lower OPEN[{rule!r}] to {count} here and in AGENTS.md")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
