#!/usr/bin/env python3
"""Thin orchestrator for reproducible parameter sweeps.

On-axis 1D sweeps use sgl_canonical_sgl_image. Off-axis observer-distance
sweeps use sgl_true_2d_sgl_image, because the 1D symmetry-reduced path rejects
nonzero observer-distance.

Edit the configuration block below, then run:

    python3 experiments/parameter_sweep.py
    python3 experiments/parameter_sweep.py --threads 8

Thread count is passed as OMP_NUM_THREADS. It is not a C++ CLI flag.

This script does not perform physics. It only builds CLI arguments, runs the
existing C++ executable, and records outputs/metadata.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# User-editable configuration
# ---------------------------------------------------------------------------

# Paths relative to the repository root (directory containing this file's parent).
EXECUTABLE_1D = "build/sgl_canonical_sgl_image"
EXECUTABLE_2D = "build/sgl_true_2d_sgl_image"

# Fixed parameters for every on-axis 1D run. Keys are CLI flag names WITHOUT
# the leading "--" and MUST match sgl_canonical_sgl_image --help.
BASE_PARAMS_1D = {
    "ray-count": 31,
    "azimuth-count": 360,
    "resolution": 256,
    "extent": 0.8,
    "b-min": 4.0,
    "b-max": 20.0,
    "step-size": 0.01,
    "max-steps": 300000,
    "observer-hit-tolerance": 1e-6,
    "max-root-iterations": 60,
    # For ray-model=parallel, this is the launch-plane distance (not a point-source range).
    "source-distance": 30.0,
    "observer-axial-distance": 30.0,
    # Perpendicular distance from the focal line / optical axis (0 = on-axis).
    "observer-distance": 0.0,
    # Finite source-distance samples use point-source rays; "inf" overrides to parallel.
    "ray-model": "parallel",
}

# Off-axis / true-2D parameters. Keys MUST match sgl_true_2d_sgl_image --help.
# N=11 is the recommended Cartesian search density: cell width ~3.6, enough
# distinct launch-plane azimuths without the 21x21 search tax.
BASE_PARAMS_2D = {
    "samples-per-axis": 71,
    "resolution": 64,
    "extent": 0.8,
    "b-max": 20.0,
    "step-size": 0.01,
    "max-steps": 300000,
    "source-distance": 10.0,
    "observer-axial-distance": 10.0,
    "observer-distance": 0.0,
    "observer-hit-tolerance": 1e-6,
    "max-root-iterations": 12,
}

# Active sweep — change only these three fields to switch experiments.
# Use the string "inf" in a source-distance sweep for source-at-infinity (parallel rays).
#SWEEP_NAME = "source_distance"
#SWEEP_PARAMETER = "source-distance"
#SWEEP_VALUES = [20, 50, 100, 200, 500, 1000, "inf"]

# Off-axis sweep. The 1D canonical executable rejects observer-distance != 0,
# so this automatically selects sgl_true_2d_sgl_image.
SWEEP_NAME = "observer_distance"
SWEEP_PARAMETER = "observer-distance"
SWEEP_VALUES = [0.0, 0.5, 1.0, 1.5, 2.0]

OUTPUT_ROOT = "outputs/sweeps"

# OpenMP thread count for each C++ run. Integer >= 1, or None to leave
# OMP_NUM_THREADS unset (OpenMP then uses all logical cores). Override at the
# command line with --threads. On hybrid CPUs, physical-core counts are usually
# more efficient than SMT (for example 8 vs 12 on an i5-13420H).
NUM_THREADS: int | None = 8

SOURCE_AT_INFINITY = "inf"

# Known CLI parameters (excluding --output-dir and --help).
KNOWN_CLI_PARAMS_1D = frozenset(
    {
        "ray-count",
        "azimuth-count",
        "resolution",
        "extent",
        "b-min",
        "b-max",
        "step-size",
        "max-steps",
        "source-distance",
        "observer-axial-distance",
        "observer-distance",
        "ray-model",
        "observer-hit-tolerance",
        "max-root-iterations",
    }
)

KNOWN_CLI_PARAMS_2D = frozenset(
    {
        "samples-per-axis",
        "resolution",
        "extent",
        "b-max",
        "step-size",
        "max-steps",
        "source-distance",
        "observer-axial-distance",
        "observer-distance",
        "observer-hit-tolerance",
        "max-root-iterations",
    }
)

# ---------------------------------------------------------------------------
# Orchestrator (do not edit for ordinary sweeps)
# ---------------------------------------------------------------------------


def resolve_num_threads(cli_threads: int | None) -> int | None:
    """CLI --threads overrides NUM_THREADS. None leaves OMP_NUM_THREADS unset."""
    value = NUM_THREADS if cli_threads is None else cli_threads
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or value < 1:
        raise SystemExit(
            f"Thread count must be an integer >= 1, or None for OpenMP default; got {value!r}"
        )
    return value


def subprocess_env(num_threads: int | None) -> dict[str, str]:
    env = os.environ.copy()
    if num_threads is not None:
        env["OMP_NUM_THREADS"] = str(num_threads)
    return env


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run a configured parameter sweep. Physics flags live in the configuration "
            "block at the top of this file. --threads only sets OMP_NUM_THREADS."
        )
    )
    parser.add_argument(
        "--threads",
        type=int,
        default=None,
        metavar="N",
        help=(
            "OpenMP thread count for the C++ executable (sets OMP_NUM_THREADS). "
            f"Overrides NUM_THREADS={NUM_THREADS!r} in the config block. "
            "Omit to use that config value; the C++ binaries have no --threads flag."
        ),
    )
    return parser.parse_args(argv)


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def format_value_dirname(value: Any) -> str:
    """Filesystem-safe directory name for an integer or floating-point value."""
    if isinstance(value, bool):
        raise TypeError("boolean sweep values are not supported")
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        text = f"{value:.12g}"
        text = text.replace("+", "")
        text = text.replace(".", "p")
        text = text.replace("-", "m")
        return text
    text = str(value)
    safe = []
    for ch in text:
        if ch.isalnum() or ch in ("-", "_"):
            safe.append(ch)
        elif ch in (".",):
            safe.append("p")
        else:
            safe.append("_")
    return "".join(safe) or "value"


def is_source_at_infinity(value: Any) -> bool:
    if isinstance(value, str) and value.strip().lower() in {SOURCE_AT_INFINITY, "infinity"}:
        return True
    if isinstance(value, float) and math.isinf(value):
        return True
    return False


def uses_2d_path() -> bool:
    """Off-axis geometry is invalid for the 1D symmetry-reduced executable."""
    if SWEEP_PARAMETER == "observer-distance":
        return True
    observer_distance = BASE_PARAMS_1D.get("observer-distance", 0.0)
    try:
        return float(observer_distance) != 0.0
    except (TypeError, ValueError):
        return False


def active_executable_relpath() -> str:
    return EXECUTABLE_2D if uses_2d_path() else EXECUTABLE_1D


def active_base_params() -> dict[str, Any]:
    return dict(BASE_PARAMS_2D if uses_2d_path() else BASE_PARAMS_1D)


def active_known_params() -> frozenset[str]:
    return KNOWN_CLI_PARAMS_2D if uses_2d_path() else KNOWN_CLI_PARAMS_1D


def validate_configuration() -> None:
    known = active_known_params()
    base_params = active_base_params()
    unknown_base = sorted(set(base_params) - known)
    if unknown_base:
        raise SystemExit(
            "BASE_PARAMS contains unknown CLI parameter(s): "
            + ", ".join(unknown_base)
            + "\nAllowed: "
            + ", ".join(sorted(known))
        )
    if SWEEP_PARAMETER not in known:
        raise SystemExit(
            f"SWEEP_PARAMETER '{SWEEP_PARAMETER}' is not a known CLI parameter.\n"
            "Allowed: " + ", ".join(sorted(known))
        )
    if not SWEEP_NAME:
        raise SystemExit("SWEEP_NAME must be a non-empty string")
    if not SWEEP_VALUES:
        raise SystemExit("SWEEP_VALUES must contain at least one value")
    for value in SWEEP_VALUES:
        if is_source_at_infinity(value) and SWEEP_PARAMETER != "source-distance":
            raise SystemExit(
                f"Sweep value '{value}' (source at infinity / parallel rays) is only "
                "valid when SWEEP_PARAMETER == 'source-distance'."
            )


def build_command(
    executable: Path,
    output_dir: Path,
    params: dict[str, Any],
) -> list[str]:
    cmd = [str(executable), "--output-dir", str(output_dir)]
    for key in sorted(params):
        cmd.extend([f"--{key}", str(params[key])])
    return cmd


def write_run_metadata(
    path: Path,
    *,
    sweep_name: str,
    sweep_parameter: str,
    sweep_value: Any,
    command: list[str],
    params: dict[str, Any],
    output_dir: Path,
    num_threads: int | None,
) -> None:
    payload = {
        "sweep_name": sweep_name,
        "sweep_parameter": sweep_parameter,
        "sweep_value": sweep_value,
        "method": "true_2d" if uses_2d_path() else "symmetry_reduced_1d",
        "command": command,
        "command_string": (
            f"OMP_NUM_THREADS={num_threads} " if num_threads is not None else ""
        )
        + " ".join(shlex.quote(part) for part in command),
        "effective_parameters": params,
        "output_dir": str(output_dir),
        "source_at_infinity": is_source_at_infinity(sweep_value),
        "omp_num_threads": num_threads,
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def prepare_run_params(sweep_value: Any) -> tuple[dict[str, Any], str, Any]:
    """Return (cli_params, directory_name, metadata_sweep_value)."""
    params = active_base_params()
    if is_source_at_infinity(sweep_value):
        # Parallel-ray model stands in for a source at infinity. source-distance remains
        # the finite launch-plane distance from BASE_PARAMS.
        params["ray-model"] = "parallel"
        return params, SOURCE_AT_INFINITY, SOURCE_AT_INFINITY

    params[SWEEP_PARAMETER] = sweep_value
    return params, format_value_dirname(sweep_value), sweep_value


def run_one(
    executable: Path,
    sweep_dir: Path,
    sweep_value: Any,
    num_threads: int | None,
) -> dict[str, Any]:
    params, dirname, metadata_value = prepare_run_params(sweep_value)

    run_dir = sweep_dir / dirname
    run_dir.mkdir(parents=True, exist_ok=True)

    command = build_command(executable, run_dir, params)
    env = subprocess_env(num_threads)
    command_string = " ".join(shlex.quote(part) for part in command)
    if num_threads is not None:
        command_string = f"OMP_NUM_THREADS={num_threads} {command_string}"
    print(f"\n=== {SWEEP_NAME}: {SWEEP_PARAMETER}={metadata_value} ===", flush=True)
    print(command_string, flush=True)

    completed = subprocess.run(command, capture_output=True, text=True, env=env)
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n", flush=True)
    if completed.stderr:
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n", file=sys.stderr, flush=True)

    stdout_path = run_dir / "executable_stdout.txt"
    stderr_path = run_dir / "executable_stderr.txt"
    stdout_path.write_text(completed.stdout, encoding="utf-8")
    stderr_path.write_text(completed.stderr, encoding="utf-8")

    metadata_path = run_dir / "run_metadata.json"
    write_run_metadata(
        metadata_path,
        sweep_name=SWEEP_NAME,
        sweep_parameter=SWEEP_PARAMETER,
        sweep_value=metadata_value,
        command=command,
        params=params,
        output_dir=run_dir,
        num_threads=num_threads,
    )

    if completed.returncode != 0:
        raise SystemExit(
            f"Executable failed for {SWEEP_PARAMETER}={metadata_value} "
            f"(exit code {completed.returncode}).\n"
            f"Command: {command_string}\n"
            f"Stdout saved to: {stdout_path}\n"
            f"Stderr saved to: {stderr_path}"
        )

    return {
        "sweep_name": SWEEP_NAME,
        "sweep_parameter": SWEEP_PARAMETER,
        "sweep_value": metadata_value,
        "source_at_infinity": is_source_at_infinity(metadata_value),
        "method": "true_2d" if uses_2d_path() else "symmetry_reduced_1d",
        "output_dir": str(run_dir),
        "exit_code": completed.returncode,
        "command": command_string,
        "omp_num_threads": num_threads,
        **{f"param_{key}": params[key] for key in sorted(params)},
    }


def write_summary_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        raise SystemExit("internal error: no summary rows")
    fieldnames = list(rows[0].keys())
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    args = parse_args()
    num_threads = resolve_num_threads(args.threads)
    validate_configuration()

    root = repo_root()
    executable = (root / active_executable_relpath()).resolve()
    if not executable.is_file():
        raise SystemExit(
            f"Executable not found: {executable}\n"
            "Build first with: cmake -B build -S . && cmake --build build"
        )

    sweep_dir = (root / OUTPUT_ROOT / SWEEP_NAME).resolve()
    sweep_dir.mkdir(parents=True, exist_ok=True)

    thread_desc = (
        f"{num_threads} (OMP_NUM_THREADS)"
        if num_threads is not None
        else "OpenMP default (all logical cores)"
    )
    print(
        f"Sweep '{SWEEP_NAME}' using "
        f"{'true 2D' if uses_2d_path() else '1D symmetry-reduced'} "
        f"executable: {executable}",
        flush=True,
    )
    print(f"Threads: {thread_desc}", flush=True)

    rows: list[dict[str, Any]] = []
    for value in SWEEP_VALUES:
        rows.append(run_one(executable, sweep_dir, value, num_threads))

    summary_path = sweep_dir / "summary.csv"
    write_summary_csv(summary_path, rows)
    print(f"\nWrote sweep summary: {summary_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
