"""Runs a compiler command and reports what it cost: wall time and peak resident memory.

    python3 -I budget.py --limit-mb N [--label text] -- <compiler command...>

The command fails the test when it fails or when its peak memory exceeds the limit. Time is reported, never
limited: it depends on the machine and on what else ctest runs in parallel, while peak memory is stable for a
given compiler. Peak memory is the maximum over the processes the command waited for (the compiler driver and
the compiler proper), as the kernel accounts it.
"""

import argparse
import resource
import subprocess
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--limit-mb", type=float, required=True, help="peak resident memory the command may use")
    parser.add_argument("--label", default="compile", help="what is being measured, for the report line")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="the compiler command, after --")
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if not command:
        parser.error("no command given")

    started = time.perf_counter()
    completed = subprocess.run(command, check=False)
    elapsed = time.perf_counter() - started
    peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss  # KiB on Linux, bytes on macOS
    peak_mb = peak / (1024 * 1024) if sys.platform == "darwin" else peak / 1024

    print(f"budget {arguments.label}: {elapsed:.2f} s, {peak_mb:.0f} MB (limit {arguments.limit_mb:.0f} MB)")
    if completed.returncode != 0:
        print(f"budget {arguments.label}: the command failed with status {completed.returncode}")
        return 1
    if peak_mb > arguments.limit_mb:
        print(f"budget {arguments.label}: over the memory limit")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
