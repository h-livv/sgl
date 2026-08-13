#!/usr/bin/env python3
"""Thin orchestrator for reproducible parameter sweeps of sgl_canonical_sgl_image.

Edit the configuration block below, then run:

    python3 experiments/parameter_sweep.py

This script does not perform physics. It only builds CLI arguments, runs the
existing C++ executable, and records outputs/metadata.
"""

from __future__ import annotations

import csv
import json
import math
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# User-editable configuration
# ---------------------------------------------------------------------------

# Path relative to the repository root (directory containing this file's parent).
EXECUTABLE = "build/sgl_canonical_sgl_image"

# Fixed parameters for every run in the sweep. Keys are CLI flag names WITHOUT
# the leading "--" and MUST match sgl_canonical_sgl_image --help.
BASE_PARAMS = {
    "ray-count": 101,
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

# Active sweep — change only these three fields to switch experiments.
# Use the string "inf" in a source-distance sweep for source-at-infinity (parallel rays).
#SWEEP_NAME = "source_distance"
#SWEEP_PARAMETER = "source-distance"
#SWEEP_VALUES = [20, 50, 100, 200, 500, 1000, "inf"]

# Alternate example: sweep perpendicular distance from the focal line.
SWEEP_NAME = "observer_distance"
SWEEP_PARAMETER = "observer-distance"
SWEEP_VALUES = [0, 5, 10]

OUTPUT_ROOT = "outputs/sweeps"

SOURCE_AT_INFINITY = "inf"

# Known CLI parameters accepted by sgl_canonical_sgl_image (excluding --output-dir,
# which the orchestrator sets per run, and --help).
KNOWN_CLI_PARAMS = frozenset(
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

# ---------------------------------------------------------------------------
# Orchestrator (do not edit for ordinary sweeps)
# ---------------------------------------------------------------------------


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


def validate_configuration() -> None:
    unknown_base = sorted(set(BASE_PARAMS) - KNOWN_CLI_PARAMS)
    if unknown_base:
        raise SystemExit(
            "BASE_PARAMS contains unknown CLI parameter(s): "
            + ", ".join(unknown_base)
            + "\nAllowed: "
            + ", ".join(sorted(KNOWN_CLI_PARAMS))
        )
    if SWEEP_PARAMETER not in KNOWN_CLI_PARAMS:
        raise SystemExit(
            f"SWEEP_PARAMETER '{SWEEP_PARAMETER}' is not a known CLI parameter.\n"
            "Allowed: " + ", ".join(sorted(KNOWN_CLI_PARAMS))
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
) -> None:
    payload = {
        "sweep_name": sweep_name,
        "sweep_parameter": sweep_parameter,
        "sweep_value": sweep_value,
        "command": command,
        "command_string": " ".join(shlex.quote(part) for part in command),
        "effective_parameters": params,
        "output_dir": str(output_dir),
        "source_at_infinity": is_source_at_infinity(sweep_value),
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def prepare_run_params(sweep_value: Any) -> tuple[dict[str, Any], str, Any]:
    """Return (cli_params, directory_name, metadata_sweep_value)."""
    params = dict(BASE_PARAMS)
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
) -> dict[str, Any]:
    params, dirname, metadata_value = prepare_run_params(sweep_value)

    run_dir = sweep_dir / dirname
    run_dir.mkdir(parents=True, exist_ok=True)

    command = build_command(executable, run_dir, params)
    command_string = " ".join(shlex.quote(part) for part in command)
    print(f"\n=== {SWEEP_NAME}: {SWEEP_PARAMETER}={metadata_value} ===", flush=True)
    print(command_string, flush=True)

    completed = subprocess.run(command, capture_output=True, text=True)
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
        "output_dir": str(run_dir),
        "exit_code": completed.returncode,
        "command": command_string,
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
    validate_configuration()

    root = repo_root()
    executable = (root / EXECUTABLE).resolve()
    if not executable.is_file():
        raise SystemExit(
            f"Executable not found: {executable}\n"
            "Build first with: cmake -B build -S . && cmake --build build"
        )

    sweep_dir = (root / OUTPUT_ROOT / SWEEP_NAME).resolve()
    sweep_dir.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, Any]] = []
    for value in SWEEP_VALUES:
        rows.append(run_one(executable, sweep_dir, value))

    summary_path = sweep_dir / "summary.csv"
    write_summary_csv(summary_path, rows)
    print(f"\nWrote sweep summary: {summary_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
