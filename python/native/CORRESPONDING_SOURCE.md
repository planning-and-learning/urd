# Corresponding source

`pyurd-native` wheels bundle the native dependencies in binary form. For such a wheel, download
`pyurd_native-<VERSION>-corresponding-source.tar.gz` from the matching
[`native-v<VERSION>` GitHub release](https://github.com/planning-and-learning/urd/releases).
It contains pyurd-native's source distribution, with the build scripts and patches of the dependencies, together with
the exact upstream native dependency sources, including required Git submodules.
The source distribution alone is also on the [PyPI files page](https://pypi.org/project/pyurd-native/#files).

`pyurd` wheels hold urd's own headers and CMake package only; their source distribution is on the
[PyPI files page](https://pypi.org/project/pyurd/#files).

[`SOURCES.json`](SOURCES.json) is the canonical, machine-readable manifest of
the repositories, revisions, archive URLs, and checksums used to build those
dependencies.
