#!/usr/bin/env python3
"""Every container benchmarks/requests.json lists under `tested` has its property tests in the binary.

    containers_coverage.py <test_containers>

The suites are named properties_<container>_<chunk>/Properties/... (CMakeLists.txt, urd_per_container); a container
without one was left out of the build, and is reported.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

REQUESTS = Path(__file__).resolve().parents[1] / "benchmarks" / "requests.json"


def main() -> int:
    """Compares the containers tested with those listed."""
    binary = sys.argv[1]
    listed = subprocess.run([binary, "--gtest_list_tests"], check=True, capture_output=True, text=True).stdout
    found = {match.group(1) for match in re.finditer(r"^properties_(\w+?)_\d+/Properties/", listed, re.MULTILINE)}
    wanted = set(json.loads(REQUESTS.read_text())["tested"])
    for container in sorted(wanted - found):
        print(f"no property tests: {container}")
    for container in sorted(found - wanted):
        print(f"property tests for a container requests.json does not list: {container}")
    return 0 if found == wanted else 1


if __name__ == "__main__":
    sys.exit(main())
