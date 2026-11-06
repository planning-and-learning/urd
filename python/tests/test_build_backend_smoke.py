#!/usr/bin/env python3
# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import importlib.util
import os
import sys
import tempfile
import types
from collections.abc import Callable
from pathlib import Path
from typing import Any, Protocol, runtime_checkable

import pytest

ROOT = Path(__file__).resolve().parents[2]



@runtime_checkable
class Backend(Protocol):
    """What these tests call: pyurd's own parts of its build backend (the rest is pybrokkr's, tested there)."""

    BACKEND: Any

    def should_strip(self, path: Path) -> bool: ...


def load_backend() -> Backend:
    fake_scikit_build_core = types.ModuleType("scikit_build_core")
    fake_scikit_build_core.__dict__["build"] = types.SimpleNamespace()
    sys.modules["scikit_build_core"] = fake_scikit_build_core

    spec = importlib.util.spec_from_file_location(
        "pyurd_build_backend", ROOT / "python" / "pyurd_build_backend.py"
    )
    assert spec is not None
    assert spec.loader is not None
    assert spec.origin is not None
    module = types.ModuleType(spec.name)
    module.__file__ = spec.origin
    module.__loader__ = spec.loader
    module.__package__ = spec.parent
    module.__spec__ = spec
    spec.loader.exec_module(module)
    if not isinstance(module, Backend):
        msg = "the build backend lacks a helper these tests call"
        raise TypeError(msg)
    return module


def with_env(name: str, value: str | None) -> str | None:
    old_value = os.environ.get(name)
    if value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = value
    return old_value


def restore_env(name: str, old_value: str | None) -> None:
    if old_value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = old_value


def expect_raises(expected: type[BaseException], func: Callable[[], object]) -> None:
    try:
        func()
    except expected:
        return
    msg = f"expected {expected.__name__}"
    raise AssertionError(msg)


def test_job_count(backend: Backend) -> None:
    old = with_env("URD_JOBS", None)
    try:
        assert backend.BACKEND.num_jobs() == (os.cpu_count() or 1)
        os.environ["URD_JOBS"] = "6"
        assert backend.BACKEND.num_jobs() == 6
        for value in ("0", "-1", "many"):
            os.environ["URD_JOBS"] = value
            expect_raises(ValueError, backend.BACKEND.num_jobs)
    finally:
        restore_env("URD_JOBS", old)


def test_strip_candidate_detection(backend: Backend) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        assert backend.should_strip(root / "module.so")
        assert backend.should_strip(root / "module.cpython-312-x86_64-linux-gnu.so")
        assert backend.should_strip(root / "libtbb.so.12.16")
        assert backend.should_strip(root / "module.dylib")
        assert backend.should_strip(root / "module.pyd")
        assert backend.should_strip(root / "module.dll")
        assert backend.should_strip(root / "bin" / "hydra_pmi_proxy")
        assert backend.should_strip(root / "bin" / "mpiexec")
        assert not backend.should_strip(root / "module.py")
        assert not backend.should_strip(root / "module.so.txt")
        assert not backend.should_strip(root / "module.so.")
        assert not backend.should_strip(root / "module.so.12a")
        assert not backend.should_strip(root / "module.so.12.16a")
        assert not backend.should_strip(root / "libexec" / "mpiexec")


@pytest.fixture
def backend() -> Backend:
    return load_backend()


def main() -> None:
    backend = load_backend()
    test_job_count(backend)
    test_strip_candidate_detection(backend)


if __name__ == "__main__":
    main()
