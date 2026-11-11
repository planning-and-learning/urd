# Copyright (C) 2026 Dominik Drexler
# SPDX-License-Identifier: GPL-3.0-or-later

"""``python -m pyurd``: print where urd's headers and CMake package are."""

import argparse

import pyurd


def main() -> None:
    """Print the requested path or version."""
    parser = argparse.ArgumentParser(
        prog="python -m pyurd",
        description="Print discovery paths for urd's headers and CMake package.",
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--prefix",
        action="store_true",
        help="Print the prefix to put on CMAKE_PREFIX_PATH.",
    )
    group.add_argument(
        "--include-dir",
        action="store_true",
        help="Print the bundled C++ header include directory.",
    )
    group.add_argument(
        "--cmake-dir",
        action="store_true",
        help="Print the directory containing urdConfig.cmake.",
    )
    group.add_argument(
        "--version",
        action="store_true",
        help="Print the pyurd version.",
    )
    args = parser.parse_args()

    if args.prefix:
        print(pyurd.cmake_prefix())
    elif args.include_dir:
        print(pyurd.include_dir())
    elif args.cmake_dir:
        print(pyurd.cmake_dir())
    elif args.version:
        print(pyurd.__version__)


if __name__ == "__main__":
    main()
