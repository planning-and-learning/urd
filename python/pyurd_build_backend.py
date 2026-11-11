# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later

"""PEP 517 build backend of pyurd: pybrokkr's backend, building against pyurd-native's prefix (the dependencies urd's
headers include)."""

from pybrokkr.build_backend import ProviderBackend

# pyurd has no compiled module, so no stubs.
ProviderBackend(
    package="pyurd",
    providers=("pyurd_native",),
    prefix_defines={"pyurd_native": "URD_NATIVE_PREFIX"},
    jobs_env="URD_JOBS",
    strip_env="URD_STRIP_WHEEL",
    fix_stubs=False,
).install_hooks(globals())
