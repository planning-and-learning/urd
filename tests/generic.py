"""The glue names no specific tag: what a tag means is declared beside the tag (urd/tags.hpp), in the rules
(urd/rules.hpp, urd/incompatible.hpp) or by a container's declaration, and the glue only combines declarations.

    python3 -I generic.py <repository root>

Fails on a glue file that names a tag that is no category's root (tag::frozen, urd::small, has<S, sorted>). Roots
name categories, which the glue may iterate. Reports each place.
"""

import re
import sys
from pathlib import Path

GLUE = ["include/urd/property_set.hpp", "include/urd/properties.hpp", "include/urd/registry/select.hpp",
        "include/urd/registry/scan.hpp", "include/urd/registry/table.hpp", "include/urd/registry/declarations.hpp",
        "include/urd/registry/declaration.hpp", "include/urd/registry/derived.hpp", "include/urd/registry/features.hpp",
        "include/urd/registry/facts.hpp", "include/urd/store.hpp"]


def tags(root: Path) -> tuple[set[str], set[str]]:
    """Every tag's name, and those that are roots, from urd/tags.hpp."""
    text = (root / "include/urd/tags.hpp").read_text()
    names = set(re.findall(r"^struct (\w+)(?: : \w+)?\n\{\n(?:    using root = \w+;\n)?    static constexpr tag value", text, re.M))
    roots = set(re.findall(r"^struct (\w+)\n\{\n    using root = \1;", text, re.M))
    return names, roots


def main() -> int:
    root = Path(sys.argv[1])
    names, roots = tags(root)
    specific = names - roots
    pattern = re.compile(r"\b(?:tag::|urd::|has<[^>]*?\b)(" + "|".join(sorted(specific, key=len, reverse=True)) + r")\b")
    found = 0
    for relative in GLUE:
        for number, line in enumerate((root / relative).read_text().splitlines(), 1):
            if line.lstrip().startswith("//"):
                continue
            for match in pattern.finditer(line):
                print(f"{relative}:{number}: names the tag {match.group(1)}; declare what it means beside the tag")
                found += 1
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
