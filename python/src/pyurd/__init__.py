# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later

"""urd's headers and the native dependencies of the planning projects: the paths of the bundled prefix."""

import tomllib as _tomllib
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path


def _has_urd_headers(path: Path) -> bool:
    return (path / "include" / "urd").is_dir()


def source_version() -> str:
    for parent in Path(__file__).resolve().parents:
        pyproject = parent / "pyproject.toml"
        if not pyproject.exists():
            continue

        project_version = _tomllib.loads(pyproject.read_text(encoding="utf-8")).get("project", {}).get("version")
        if project_version is not None:
            return project_version

    return "0.0.0"


try:
    __version__ = version("pyurd")
except PackageNotFoundError:
    __version__ = source_version()


def native_prefix() -> Path:
    """Return the directory containing bundled native headers and libraries."""
    package_dir = Path(__file__).resolve().parent
    if _has_urd_headers(package_dir):
        return package_dir
    for parent in package_dir.parents:
        if (parent / "pyproject.toml").exists() and _has_urd_headers(parent):
            return parent

    return package_dir


def include_dir() -> Path:
    """Return the bundled C++ header include directory."""
    return native_prefix() / "include"


def library_dirs() -> tuple[Path, ...]:
    """Return existing bundled native library directories."""
    prefix = native_prefix()
    return tuple(path for path in (prefix / "lib", prefix / "lib64") if path.is_dir())


def cmake_dirs() -> tuple[Path, ...]:
    """Return existing bundled CMake package directories."""
    return tuple(path / "cmake" for path in library_dirs() if (path / "cmake").is_dir())


def cmake_prefix() -> Path:
    """Return the prefix to put on CMAKE_PREFIX_PATH.

    find_package then finds urd and its bundled dependencies there.
    """
    return native_prefix()


def cmake_dir() -> Path:
    """Return the directory containing urdConfig.cmake."""
    for cmake_packages_dir in cmake_dirs():
        candidate = cmake_packages_dir / "urd"
        if (candidate / "urdConfig.cmake").is_file():
            return candidate

    msg = (
        "urdConfig.cmake not found under "
        f"{[str(path) for path in cmake_dirs()] or native_prefix()}; "
        "the installed pyurd is too old or incomplete."
    )
    raise FileNotFoundError(msg)


__all__ = [
    "__version__",
    "cmake_dir",
    "cmake_dirs",
    "cmake_prefix",
    "include_dir",
    "library_dirs",
    "native_prefix",
]
