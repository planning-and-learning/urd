"""Round trips through urd's nanobind casters: a Store converts as the standard container of its kind does, a flat form
of a record as its std counterpart, and a view as a list."""

import importlib.util
import sys
from pathlib import Path


def load(extension: Path):
    spec = importlib.util.spec_from_file_location("urd_casters_test", extension)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    module = load(Path(sys.argv[1]).resolve())
    assert module.sequence([3, 1, 2]) == [3, 1, 2]
    assert module.sequence(()) == []
    assert module.stable_sequence(range(5)) == [0, 1, 2, 3, 4]
    assert module.set({3, 1, 2}) == {1, 2, 3}
    assert module.set([1, 1, 2]) == {1, 2}
    assert module.sorted_set({5, 4}) == {4, 5}
    assert module.map({"on": 2, "clear": 1}) == {"on": 2, "clear": 1}
    assert module.sorted_map({2: "b", 1: "a"}) == {1: "a", 2: "b"}
    assert module.nested([[1, 2], [], [3]]) == [[1, 2], [], [3]]
    assert module.positional_set([3, 1, 3]) == {1, 3}
    assert module.sizes([[1, 2], [], [3]]) == [2, 0, 1]
    for function, bad in ((module.sequence, ["x"]), (module.map, {1: 2}), (module.set, "abc")):
        try:
            function(bad)
        except TypeError:
            pass
        else:
            raise AssertionError(f"{function.__name__} accepted {bad!r}")
    assert module.array_view() == [1, 2, 3]
    assert module.pair_view() == (4, 5)
    assert module.nested_view() == [(6, 7), (8, 9)]
    assert module.roundtrip_pair((4, 5)) == (4, 5)
    assert module.roundtrip_pair([-3, 4]) == (-3, 4)
    assert module.roundtrip_optional_pairs([(1, 2), None, (-3, 4)]) == [(1, 2), None, (-3, 4)]
    assert module.roundtrip_optional_pairs([]) == []
    for value in ((), (1,), (1, 2, 3), ("bad", 2), (1, "bad")):
        for function, argument in ((module.roundtrip_pair, value), (module.roundtrip_optional_pairs, [value])):
            try:
                function(argument)
            except TypeError:
                pass
            else:
                raise AssertionError(f"invalid pair accepted: {value!r}")
    for value in (42, 3.5, -7):
        result = module.roundtrip_variant(value)
        assert type(result) is type(value)
        assert result == value
    print("casters ok")


if __name__ == "__main__":
    main()
