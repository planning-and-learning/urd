# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later

"""PEP 517 build backend of pyurd: builds the native dependencies (src/) into the prefix the wheel bundles,
then the wheel through pybrokkr's backend."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from pybrokkr.build_backend import ProviderBackend, is_disabled, is_native_library

ROOT_DIR = Path(__file__).resolve().parent.parent
URD_BUILD_DIR = ROOT_DIR / "dependencies-build"


def _native_prefix() -> Path:
    return Path(os.environ.get("URD_NATIVE_PREFIX", ROOT_DIR / "dependencies-install")).resolve()


def _build_type() -> str:
    return os.environ.get("URD_BUILD_TYPE", "Release")


def configure_and_install_dependencies() -> None:
    """Build and install the native dependencies (src/) into the native prefix, unless disabled."""
    if is_disabled(os.environ.get("URD_BUILD_NATIVE", "ON")):
        return

    cmake = shutil.which("cmake")
    if cmake is None:
        msg = "cmake is required to build pyurd"
        raise RuntimeError(msg)

    native_prefix = _native_prefix()
    if (
        "URD_NATIVE_PREFIX" in os.environ
        and native_prefix.exists()
        and (not native_prefix.is_dir() or any(native_prefix.iterdir()))
    ):
        msg = (
            "URD_NATIVE_PREFIX must be empty when URD_BUILD_NATIVE is enabled; "
            "set URD_BUILD_NATIVE=OFF to package an existing prefix"
        )
        raise RuntimeError(msg)

    default_native_prefix = ROOT_DIR / "dependencies-install"
    generated_dirs = [URD_BUILD_DIR]
    if "URD_NATIVE_PREFIX" not in os.environ:
        generated_dirs.append(default_native_prefix)
    for generated_dir in generated_dirs:
        if generated_dir.is_symlink():
            msg = f"refusing to clean symlinked build directory: {generated_dir}"
            raise RuntimeError(msg)
        if generated_dir.exists():
            shutil.rmtree(generated_dir)

    URD_BUILD_DIR.mkdir(parents=True, exist_ok=True)

    jobs = BACKEND.num_jobs()
    cmake_args = [
        cmake,
        "-S",
        str(ROOT_DIR / "src"),
        "-B",
        str(URD_BUILD_DIR),
        f"-DCMAKE_BUILD_TYPE={_build_type()}",
        f"-DCMAKE_INSTALL_PREFIX={native_prefix}",
        "-DCMAKE_INSTALL_LIBDIR=lib",
        f"-DURD_JOBS={jobs}",
        f"-DPython_EXECUTABLE={sys.executable}",
    ]

    subprocess.run(cmake_args, cwd=ROOT_DIR, check=True)
    subprocess.run([cmake, "--build", str(URD_BUILD_DIR), f"-j{jobs}"], cwd=ROOT_DIR, check=True)
    subprocess.run([cmake, "--install", str(URD_BUILD_DIR)], cwd=ROOT_DIR, check=True)


def should_strip(path: Path) -> bool:
    """Whether ``path`` is a native library whose symbols the wheel strips, or one of the bundled MPI tools."""
    return is_native_library(path) or (path.parent.name == "bin" and path.name in {"hydra_pmi_proxy", "mpiexec"})


# pyurd has no compiled module, so no stubs: the backend only strips.
BACKEND = ProviderBackend(
    package="pyurd",
    providers=(),
    before_build=configure_and_install_dependencies,
    extra_cmake_defines=lambda: (f"-DURD_NATIVE_PREFIX={_native_prefix()}", "-DCMAKE_INSTALL_LIBDIR=lib"),
    jobs_env="URD_JOBS",
    strip_env="URD_STRIP_WHEEL",
    fix_stubs=False,
    should_strip=should_strip,
    strip_unneeded=True,
)
BACKEND.install_hooks(globals())
