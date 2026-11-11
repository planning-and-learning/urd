# pyurd-native

The native dependencies of [urd](https://github.com/planning-and-learning/urd) and of the planning libraries built
on it (yggdrasil, loki, tyr, runir), as one prefix in a platform wheel: Boost, ICU, MPICH, oneTBB, nauty, gtl, cista,
fmt, nanobind, argparse, toml++, googletest and benchmark, in the versions `native/SOURCES.json` pins.

`pyurd` (urd's headers and CMake package, one wheel for every platform) requires an exact version of it, so it is
not installed on its own. It is released only when a pinned dependency changes.

```console
python -m pyurd_native --prefix    # the prefix to put on CMAKE_PREFIX_PATH
```

The licenses of the bundled libraries are under `share/licenses/` of that prefix; `share/CORRESPONDING_SOURCE.md`
says where their sources are.
