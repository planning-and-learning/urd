"""Whether what a file holding Stores costs the compiler is constant in the number of containers registered.

    python3 -I scaling.py --label name --copies k --margin-mb m -- <compiler command...>

Runs the command twice, with the registry as it is and grown by k copies of every container (URD_REGISTRY_COPIES,
include/urd/registry/preferences.hpp: copies that selection checks as far as a real candidate that does not serve),
and fails when the grown run's peak memory exceeds the first by more than the margin. Time is reported only.
"""

import argparse
import resource
import subprocess
import sys
import time


def run(command: list[str]) -> tuple[float, float]:
    """The command's wall time and peak resident memory (MB), in a child of its own."""
    probe = [sys.executable, "-I", "-c",
             "import resource, subprocess, sys, time\n"
             "started = time.perf_counter()\n"
             "status = subprocess.run(sys.argv[1:], check=False).returncode\n"
             "peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss\n"
             "print(time.perf_counter() - started, peak / (1024 * 1024) if sys.platform == 'darwin' else peak / 1024)\n"
             "sys.exit(status)\n", *command]
    done = subprocess.run(probe, check=False, capture_output=True, text=True)
    if done.returncode != 0:
        sys.stderr.write(done.stderr[-2000:])
        raise SystemExit(f"the command failed with status {done.returncode}")
    elapsed, peak = (float(each) for each in done.stdout.split())
    return elapsed, peak


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--label", required=True)
    parser.add_argument("--copies", type=int, required=True, help="copies of every container in the grown registry")
    parser.add_argument("--margin-mb", type=float, required=True, help="growth in peak memory still counted as none")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="the compiler command, after --")
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    as_is = run(command)
    grown = run([*command, f"-DURD_REGISTRY_COPIES={arguments.copies}"])
    growth = grown[1] - as_is[1]
    print(f"scaling {arguments.label}: {as_is[0]:.2f} s, {as_is[1]:.0f} MB as registered; "
          f"{grown[0]:.2f} s, {grown[1]:.0f} MB with {arguments.copies} copies of every container ({growth:+.0f} MB)")
    if growth > arguments.margin_mb:
        print(f"scaling {arguments.label}: grows with the number of containers (margin {arguments.margin_mb:.0f} MB)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
