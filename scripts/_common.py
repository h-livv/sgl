"""Shared helpers for SGL numerical validation scripts.

All scripts invoke ``sgl_canonical_sgl_image`` and read the direct observer-hit
observable ``theta_E`` from ``run_summary.txt``. Ring radius is never inferred
from image pixels.
"""

from __future__ import annotations

import csv
import json
import math
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt

EXECUTABLE_REL = "build/sgl_canonical_sgl_image"
RESULTS_ROOT_REL = "outputs/validation"


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def executable_path() -> Path:
    path = (repo_root() / EXECUTABLE_REL).resolve()
    if not path.is_file():
        raise SystemExit(
            f"Executable not found: {path}\n"
            "Build first with: cmake -B build -S . && cmake --build build"
        )
    return path


def results_dir(test_name: str) -> Path:
    path = (repo_root() / RESULTS_ROOT_REL / test_name).resolve()
    path.mkdir(parents=True, exist_ok=True)
    return path


def parse_summary(path: Path) -> dict[str, str]:
    summary: dict[str, str] = {}
    if not path.is_file():
        raise SystemExit(f"Missing run_summary.txt: {path}")
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        summary[key] = value
    return summary


def require_float(summary: dict[str, str], key: str) -> float:
    if key not in summary:
        raise SystemExit(f"run_summary.txt missing required key '{key}'")
    return float(summary[key])


def extract_theta_E(summary: dict[str, str]) -> dict[str, float]:
    """Extract the direct numerical Einstein-ring observables from a run summary.

    Preference order for theta_E:
      1. theta_E field written by the executable
      2. selected_angular_theta
      3. atan(selected_angular_radius)
    """
    if "theta_E" in summary:
        theta = float(summary["theta_E"])
    elif "selected_angular_theta" in summary:
        theta = float(summary["selected_angular_theta"])
    elif "selected_angular_radius" in summary:
        theta = math.atan(float(summary["selected_angular_radius"]))
    else:
        raise SystemExit(
            "run_summary.txt lacks theta_E / selected_angular_theta / "
            "selected_angular_radius; cannot form a direct ring-radius observable."
        )

    rho = float(summary.get("selected_angular_radius", math.tan(theta)))
    D = float(summary.get("observer_axial_distance", "nan"))
    if "R_equiv" in summary:
        r_equiv = float(summary["R_equiv"])
    elif math.isfinite(D):
        r_equiv = D * rho
    else:
        r_equiv = float("nan")

    return {
        "theta_E": theta,
        "rho": rho,
        "R_equiv": r_equiv,
        "selected_observer_hit_b": float(summary.get("selected_observer_hit_b", "nan")),
        "selected_observer_hit_residual_u": float(
            summary.get("selected_observer_hit_residual_u", "nan")
        ),
        "selected_angular_u": float(summary.get("selected_angular_u", "nan")),
        "selected_angular_v": float(summary.get("selected_angular_v", "nan")),
        "observer_hit_candidate_count": float(
            summary.get("observer_hit_candidate_count", "nan")
        ),
        "observer_axial_distance": D,
        "source_distance": float(summary.get("source_distance", "nan")),
        "raw_image_max": float(summary.get("raw_image_max", "nan")),
    }


def build_command(executable: Path, output_dir: Path, params: dict[str, Any]) -> list[str]:
    cmd = [str(executable), "--output-dir", str(output_dir)]
    for key in sorted(params):
        cmd.extend([f"--{key}", str(params[key])])
    return cmd


def run_canonical(
    *,
    run_dir: Path,
    params: dict[str, Any],
    label: str,
) -> dict[str, Any]:
    """Run the canonical executable once and return auditable observables."""
    executable = executable_path()
    run_dir.mkdir(parents=True, exist_ok=True)

    command = build_command(executable, run_dir, params)
    command_string = " ".join(shlex.quote(part) for part in command)
    print(f"\n=== {label} ===", flush=True)
    print(command_string, flush=True)

    t0 = time.perf_counter()
    completed = subprocess.run(command, capture_output=True, text=True)
    elapsed = time.perf_counter() - t0

    (run_dir / "executable_stdout.txt").write_text(completed.stdout, encoding="utf-8")
    (run_dir / "executable_stderr.txt").write_text(completed.stderr, encoding="utf-8")

    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n", flush=True)
    if completed.stderr:
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n", flush=True)

    metadata = {
        "label": label,
        "command": command,
        "command_string": command_string,
        "effective_parameters": params,
        "output_dir": str(run_dir),
        "elapsed_seconds": elapsed,
        "exit_code": completed.returncode,
    }
    (run_dir / "run_metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    if completed.returncode != 0:
        raise SystemExit(
            f"Executable failed for {label} (exit {completed.returncode}).\n"
            f"Command: {command_string}\n"
            f"See {run_dir / 'executable_stderr.txt'}"
        )

    summary = parse_summary(run_dir / "run_summary.txt")
    observables = extract_theta_E(summary)
    result = {
        **metadata,
        **observables,
        "summary": summary,
        "ring_radius_source": summary.get("ring_radius_source", "observer_hit_root"),
    }
    (run_dir / "observables.json").write_text(
        json.dumps(
            {k: v for k, v in result.items() if k != "summary"},
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(
        f"  theta_E={observables['theta_E']:.8g}  "
        f"rho={observables['rho']:.8g}  "
        f"b={observables['selected_observer_hit_b']:.8g}  "
        f"residual_u={observables['selected_observer_hit_residual_u']:.3e}  "
        f"({elapsed:.1f}s)",
        flush=True,
    )
    return result


def write_csv(path: Path, rows: list[dict[str, Any]], fieldnames: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def save_figure(path: Path) -> None:
    plt.tight_layout()
    plt.savefig(path, dpi=140)
    plt.close()
    print(f"Wrote {path}", flush=True)


def weak_field_theta_E(*, rs: float, D: float, S: float | None) -> float:
    """Analytical weak-field Einstein angle.

    For finite S: R_E = sqrt(2 rs D S / (D+S)), theta = atan(R_E / D).
    For S -> infinity: R_E = sqrt(2 rs D).
    """
    if S is None or math.isinf(S):
        R_E = math.sqrt(2.0 * rs * D)
    else:
        R_E = math.sqrt(2.0 * rs * D * S / (D + S))
    return math.atan(R_E / D)
