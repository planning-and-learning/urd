# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later

"""The native dependencies of urd and of the libraries built on it: the paths of the bundled prefix."""

from importlib.metadata import PackageNotFoundError, version
from pathlib import Path

try:
    __version__ = version("pyurd-native")
except PackageNotFoundError:
    __version__ = "0.0.0"


def native_prefix() -> Path:
    """Return the directory containing the bundled native headers and libraries."""
    return Path(__file__).resolve().parent


def include_dir() -> Path:
    """Return the bundled C++ header include directory."""
    return native_prefix() / "include"


def library_dirs() -> tuple[Path, ...]:
    """Return existing bundled native library directories."""
    prefix = native_prefix()
    return tuple(path for path in (prefix / "lib", prefix / "lib64") if path.is_dir())


def cmake_prefix() -> Path:
    """Return the prefix to put on CMAKE_PREFIX_PATH.

    find_package then finds the bundled dependencies there.
    """
    return native_prefix()


__all__ = [
    "__version__",
    "cmake_prefix",
    "include_dir",
    "library_dirs",
    "native_prefix",
]
