#!/usr/bin/env python3
"""Dependency validation script for AncestryTree.

The script inspects the host platform for:
    * An available C99 compiler (cl, clang, or gcc depending on the OS).
    * A raylib installation discoverable via RAYLIB_HOME or pkg-config.
    * The presence of the Nuklear single-header GUI library.

Manual verification reminder:
    After this script reports success, rebuild the project and launch the
    prototype (see README) to confirm the holographic 3D window is stable.

The script exits with status code 0 when all required components are detected
and prints a concise summary that can be consumed by future CI jobs.
"""

from __future__ import annotations

import os
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass
from typing import Iterable, Optional


@dataclass
class CheckResult:
    name: str
    passed: bool
    detail: str

    def format(self) -> str:
        status = "PASS" if self.passed else "FAIL"
        return f"[{status}] {self.name}: {self.detail}"


def run_command(command: Iterable[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, capture_output=True, text=True)


def detect_compiler(system: str) -> CheckResult:
    candidates = []
    if system == "Windows":
        candidates.extend(["cl", "clang", "gcc"])
    else:
        candidates.extend(["clang", "gcc", "cc"])

    for compiler in candidates:
        path = shutil.which(compiler)
        if path:
            return CheckResult("C99 compiler", True, f"found {compiler} at {path}")
    return CheckResult("C99 compiler", False, "no suitable compiler found in PATH")


def detect_raylib(system: str) -> CheckResult:
    home = os.environ.get("RAYLIB_HOME")
    search_paths = []
    if home:
        search_paths.extend(
            [
                os.path.join(home, "lib", "raylib.lib"),
                os.path.join(home, "lib", "libraylib.a"),
                os.path.join(home, "lib64", "raylib.lib"),
                os.path.join(home, "lib64", "libraylib.a"),
            ]
        )
        if system != "Windows":
            search_paths.extend(
                [
                    os.path.join(home, "lib", "libraylib.dylib"),
                    os.path.join(home, "lib", "libraylib.so"),
                ]
            )

    existing = next((path for path in search_paths if os.path.isfile(path)), None)
    if existing:
        return CheckResult("raylib", True, f"library detected via RAYLIB_HOME ({existing})")

    # Fallback to pkg-config for Unix-like systems.
    if system != "Windows" and shutil.which("pkg-config"):
        result = run_command(["pkg-config", "--libs", "raylib"])
        if result.returncode == 0:
            return CheckResult("raylib", True, f"pkg-config reports: {result.stdout.strip()}")
        return CheckResult(
            "raylib",
            False,
            "pkg-config could not find raylib; install libraylib-dev or set RAYLIB_HOME",
        )

    if home:
        return CheckResult(
            "raylib",
            False,
            f"raylib libraries not found under RAYLIB_HOME={home}; check lib directories",
        )
    return CheckResult(
        "raylib",
        False,
        "RAYLIB_HOME not set and pkg-config unavailable; cannot locate raylib",
    )


def detect_nuklear(include_path: Optional[str]) -> CheckResult:
    candidates = []
    if include_path:
        candidates.append(os.path.join(include_path, "nuklear.h"))
    candidates.append(os.path.join("include", "external", "nuklear.h"))

    existing = next((path for path in candidates if os.path.isfile(path)), None)
    if existing:
        return CheckResult("Nuklear", True, f"header located at {existing}")
    return CheckResult(
        "Nuklear",
        False,
        "nuklear.h not found; download from https://github.com/Immediate-Mode-UI/Nuklear",
    )


def print_summary(results: Iterable[CheckResult]) -> None:
    print("Dependency Check Summary")
    print("========================")
    failed = False
    for result in results:
        print(result.format())
        failed |= not result.passed
    print("========================")
    if failed:
        print("At least one dependency check failed. See messages above.")
    else:
        print("All required dependencies detected. Proceed with build/tests.")


def main(argv: list[str]) -> int:
    system = platform.system()
    include_override = os.environ.get("NUKLEAR_INCLUDE")

    results = [
        detect_compiler(system),
        detect_raylib(system),
        detect_nuklear(include_override),
    ]

    print_summary(results)
    if any(not result.passed for result in results):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
