"""The request language, group -> category -> tag, from share/urd/vocabulary.json.

Directives:

- ``.. urd-groups::``: the four groups (kind, semantics, costs, hints), each with its test, questions and tags.
- ``.. urd-group:: <group>``: one group's categories, as a table.
- ``.. urd-heading:: <heading>``: within semantics, the categories under one reading heading, as a table.
- ``.. urd-category:: <root>``: one category: its question as a section, its diagram, the body written in the
  page, then a card per tag (with the shortest served request of docs/RESOLUTION.md stating it).
- ``.. urd-needs::``: every phrase, alphabetically, linked to its tag.
- ``.. urd-complexity:: <kind>``: the costs of a kind's representations, from share/urd/complexity.json.

Every category must appear exactly once, on its group's page (language/<group>); a warning says which, and the build with -W fails.
"""

import json
import re
from pathlib import Path

from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.statemachine import StringList
from sphinx.util import logging
from sphinx.util.nodes import nested_parse_with_titles

LOGGER = logging.getLogger(__name__)
ROOT = Path(__file__).resolve().parents[2]
VOCABULARY = json.loads((ROOT / "share/urd/vocabulary.json").read_text())
GROUPS = {g["group"]: g for g in VOCABULARY["groups"]}
HEADINGS = {h["heading"]: h for h in VOCABULARY["headings"]}
CATEGORIES = {c["category"]: c for c in VOCABULARY["categories"]}
TAGS = {t["tag"]: t for t in VOCABULARY["tags"]}
COMPLEXITY = json.loads((ROOT / "share/urd/complexity.json").read_text())
ROLES = {
    "guarantee": "a guarantee: the container keeps it",
    "demand": "a demand: you promise it about the data",
    "kind": "a kind: it makes the Store a queue, a pool or a partition",
    "request": "a request: only a request states it",
    "hint": "a hint: it changes the choice, never a guarantee",
}
_RESULTS = sorted((ROOT / "benchmarks/results").glob("*.json"))
MEASURED = json.loads(_RESULTS[0].read_text())["cells"] if _RESULTS else {}


def measured(representation: str, operation: str) -> str:
    """The measured time of an operation at 10^4 int elements (benchmarks/results), as " · 12 ns", or ""."""
    for name, result in MEASURED.items():
        parts = name.split("/")
        if parts[0] == representation and set(parts[1].split(";")) <= {"int", "-"} and parts[-1] == "10000" and operation in result.get("ns", {}):
            return f" · {result['ns'][operation]:.3g} ns"
    return ""


def served_requests() -> list[tuple[str, set[str]]]:
    """Every served row of RESOLUTION.md: the request as written and the tags it states."""
    rows = []
    for line in (ROOT / "docs/RESOLUTION.md").read_text().splitlines():
        if line.startswith("| `Store<") and "| `none`" not in line:
            request = line.split("`")[1]
            rows.append((request, set(re.findall(r"\b[a-z_]+\b", request)) & TAGS.keys()))
    return rows


SERVED = served_requests()


def shortest_request(tag: str) -> str | None:
    return min((r for r, stated in SERVED if tag in stated), key=len, default=None)


def on_page(group: str) -> list[dict]:
    """The categories of a group, in the order its page (language/<group>.rst) holds them: its writer chose it."""
    page = Path(__file__).resolve().parents[1] / "language" / f"{group}.rst"
    source = page.read_text() if page.exists() else ""
    roots = re.findall(r"^\.\. urd-category:: (\w+)", source, flags=re.MULTILINE)
    return [CATEGORIES[r] for r in roots if r in CATEGORIES and CATEGORIES[r]["group"] == group]


def category_table(roots: list[dict]) -> list[str]:
    out = [".. list-table::", "   :header-rows: 1", "   :widths: 40 45 15", "",
           "   * - Question", "     - Tags", "     - Stated"]
    for c in roots:
        out += [f"   * - {category_link(c['category'])}", f"     - {code(c['tags'])}",
                "     - yes" if c["statable"] else "     - no: derived"]
    return out


def code(names: list[str]) -> str:
    return ", ".join(f"``{n}``" for n in names)


def tag_link(tag: str) -> str:
    return f":ref:`{tag} <tag-{tag}>`"


def category_link(root: str) -> str:
    return f":ref:`{CATEGORIES[root]['question']} <category-{root}>`"


class Generated(Directive):
    """A directive whose output is reStructuredText it writes, parsed in place."""

    def lines(self) -> list[str]:
        raise NotImplementedError

    def run(self) -> list[nodes.Node]:
        container = nodes.container()
        nested_parse_with_titles(self.state, StringList(self.lines(), source="urd_language"), container)
        return container.children


class Groups(Generated):
    def lines(self) -> list[str]:
        out = [".. list-table::", "   :header-rows: 1", "   :widths: 14 30 30 26", "", "   * - Group", "     - Leaving out a tag you need",
               "     - Questions", "     - Tags"]
        for group in GROUPS.values():
            roots = on_page(group["group"])
            out += [f"   * - :doc:`{group['title']} <{group['group']}>`",
                    f"     - {group['test']}",
                    "     - " + " · ".join(c["question"] for c in roots),
                    "     - " + code([t for c in roots for t in c["tags"]])]
        return out


class Group(Generated):
    required_arguments = 1

    def lines(self) -> list[str]:
        group = self.arguments[0]
        if group not in GROUPS:
            raise self.error(f"urd-group: unknown group {group!r}")
        roots = on_page(group)
        if not roots:
            raise self.error(f"urd-group: group {group!r} holds no category on this page")
        return [*category_table(roots), "", GROUPS[group]["literature"], ""]


class Heading(Generated):
    required_arguments = 1

    def lines(self) -> list[str]:
        heading = self.arguments[0]
        if heading not in HEADINGS:
            raise self.error(f"urd-heading: unknown heading {heading!r}")
        roots = [c for c in on_page(HEADINGS[heading]["group"]) if c.get("heading") == heading]
        if not roots:
            raise self.error(f"urd-heading: heading {heading!r} holds no category on this page")
        return category_table(roots)


class Category(Generated):
    required_arguments = 1
    has_content = True

    def lines(self) -> list[str]:
        root = self.arguments[0]
        if root not in CATEGORIES:
            raise self.error(f"urd-category: unknown category {root!r}")
        category = CATEGORIES[root]
        env = self.state.document.settings.env
        if env.docname != f"language/{category['group']}":
            raise self.error(f"urd-category: {root} belongs on language/{category['group']}, not {env.docname}")
        env.urd_categories.setdefault(root, []).append(env.docname)

        question = category["question"]
        stated = "" if category["statable"] else " Not stated in a request: it follows from other tags."
        out = [f".. _category-{root}:", "", question, "-" * len(question), "",
               f"Category ``{root}``, which promises nothing: a request that leaves the question open gets it.{stated}",
               "", ".. graphviz::", "", f"   digraph {root} {{",
               "      rankdir=LR; node [fontname=sans, fontsize=10]; edge [fontname=sans, fontsize=9];",
               f'      {root} [shape=plaintext, fontcolor=gray40, label="{root}\\n(promises nothing)"];']
        for tag in category["tags"]:
            parents = [p for p in TAGS[tag]["implies"] if p in category["tags"]] or [root]
            out += [f"      {p} -> {tag};" for p in parents]
        out += ["   }", "", *self.content, ""]
        for tag in category["tags"]:
            out += self.card(TAGS[tag])
        return out

    @staticmethod
    def card(tag: dict) -> list[str]:
        name = tag["tag"]
        example = shortest_request(name) if tag["statable"] else None
        out = [f".. _tag-{name}:", "", f"``{name}``", "~" * (len(name) + 4), "", tag["description"], "",
               f"- **Role:** {ROLES[tag['role']]}.", f"- **In plain words:** {', '.join(tag['phrases'])}."]
        if "promise" in tag:
            out.append("- **Kept by:** " + ("you, about your data." if tag["promise"] == "caller" else "the container."))
        implied = [t for t in tag["implies"] if t in TAGS]
        if implied:
            out.append(f"- **Promises at least:** {code(implied)}.")
        others = [t for t in tag["excludes"] if t not in implied]
        if others:
            out.append(f"- **Instead of:** {code(others)} (one answer per question).")
        if not tag["statable"]:
            out.append("- **Not stated:** it follows from the answer to another question.")
        elif example:
            out.append(f"- **Shortest served request:** ``{example}``.")
        else:
            out.append("- **Shortest served request:** none in :doc:`/RESOLUTION`.")
        return [*out, ""]


class Needs(Generated):
    def lines(self) -> list[str]:
        out = [".. list-table::", "   :header-rows: 1", "   :widths: 50 50", "", "   * - You need", "     - Tag"]
        for phrase, tag in sorted({(p, t["tag"]) for t in TAGS.values() for p in t["phrases"]}):
            out += [f"   * - {phrase}", f"     - {tag_link(tag)} ({category_link(TAGS[tag]['category'])})"]
        return out


class Complexity(Generated):
    """``.. urd-complexity:: <kind>``: what each operation costs the representations of a kind of Store."""

    required_arguments = 1

    def lines(self) -> list[str]:
        kind = self.arguments[0]
        rows = [r for r in COMPLEXITY["representations"] if kind in r["kinds"]]
        if not rows:
            raise self.error(f"urd-complexity: no representation serves {kind!r}")
        ops = [o for o in COMPLEXITY["operations"] if any(o in r["costs"] for r in rows)]
        out = [".. list-table::", "   :header-rows: 1", "", "   * - Representation"]
        out += [f"     - {o.replace('_', ' ')}" for o in ops] + ["     - holds"]
        for r in rows:
            out.append(f"   * - ``{r['representation']}``: {r['description']}")
            for o in ops:
                cost = r["costs"].get(o)
                text = "—" if cost is None else "as its part" if cost.get("as_part") else cost["cost"].replace("*", "\\*")
                if cost is not None:
                    text += measured(r["representation"], o)
                out.append(f"     - {text}")
            out.append(f"     - {r['holds']}")
        return [*out, "", "\\* amortized, ~ expected; n elements, U the key range, r a row's length, k a delta's changes. "
                       "Where measured, the median time per operation at 10\\ :sup:`4` int elements follows "
                       "(:doc:`/BENCHMARKS`).", ""]


def init_env(app, env, docnames) -> None:
    if not hasattr(env, "urd_categories"):
        env.urd_categories = {}
    for docname in docnames:
        purge(app, env, docname)


def purge(app, env, docname) -> None:
    for pages in getattr(env, "urd_categories", {}).values():
        while docname in pages:
            pages.remove(docname)


def check(app, env) -> None:
    for root in CATEGORIES:
        pages = env.urd_categories.get(root, [])
        if len(pages) != 1:
            LOGGER.warning(f"urd-category {root}: on {len(pages)} pages, must be on exactly one (docs/language/)")


def setup(app) -> dict:
    app.add_directive("urd-groups", Groups)
    app.add_directive("urd-group", Group)
    app.add_directive("urd-heading", Heading)
    app.add_directive("urd-category", Category)
    app.add_directive("urd-needs", Needs)
    app.add_directive("urd-complexity", Complexity)
    app.connect("env-before-read-docs", init_env)
    app.connect("env-purge-doc", purge)
    app.connect("env-check-consistency", check)
    return {"version": "1", "parallel_read_safe": False}
