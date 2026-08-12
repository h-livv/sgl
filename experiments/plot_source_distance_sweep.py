#!/usr/bin/env python3
"""Plot diagnostics from the latest source_distance sweep.

Produces:
  1. Ring radius vs source distance (including S=∞ parallel-ray case)
  2. Radial intensity profiles for all distances overlaid

Usage (from repository root):

    python3 experiments/plot_source_distance_sweep.py
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SWEEP_ROOT = Path("outputs/sweeps")
SWEEP_NAME = "source_distance"
# If several matching directories exist, the most recently modified is used.
N_RADIUS_BINS = 256
INFINITY_DIRNAME = "inf"


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------


@dataclass
class SweepRun:
    source_distance: float  # math.inf for parallel / source-at-infinity
    directory: Path
    intensity: np.ndarray  # shape (height, width), row-major y,x
    u_min: float
    u_max: float
    v_min: float
    v_max: float
    ray_model: str = "point"
    is_infinity: bool = False

    @property
    def label(self) -> str:
        if self.is_infinity:
            return r"$S=\infty$ (parallel)"
        return f"S = {self.source_distance:g}"

    @property
    def width(self) -> int:
        return int(self.intensity.shape[1])

    @property
    def height(self) -> int:
        return int(self.intensity.shape[0])


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def find_latest_source_distance_sweep(root: Path) -> Path:
    """Return the newest directory named source_distance under outputs/sweeps."""
    candidates = [
        path
        for path in root.rglob(SWEEP_NAME)
        if path.is_dir() and any(path.glob("*/einstein_ring.csv"))
    ]
    if not candidates:
        conventional = root / SWEEP_NAME
        if conventional.is_dir():
            candidates = [conventional]
    if not candidates:
        raise SystemExit(
            f"No '{SWEEP_NAME}' sweep found under {root}. "
            "Run experiments/parameter_sweep.py with SWEEP_NAME='source_distance' first."
        )
    candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    return candidates[0]


def detect_infinity_run(run_dir: Path, payload: dict | None, params: dict) -> bool:
    if run_dir.name.lower() in {INFINITY_DIRNAME, "infinity"}:
        return True
    if payload:
        if payload.get("source_at_infinity") is True:
            return True
        value = payload.get("sweep_value")
        if isinstance(value, str) and value.strip().lower() in {INFINITY_DIRNAME, "infinity"}:
            return True
    model = str(params.get("ray-model", "")).lower()
    # A dedicated parallel run stored under source_distance/ is treated as S=∞.
    if model == "parallel" and run_dir.parent.name == SWEEP_NAME:
        # Only if not also a finite numeric source-distance point-model folder.
        # Finite folders are named 50/100/...; parallel infinity uses "inf".
        return run_dir.name.lower() in {INFINITY_DIRNAME, "infinity"} or bool(
            payload and payload.get("source_at_infinity")
        )
    return False


def parse_run_identity(run_dir: Path) -> tuple[float, str, bool]:
    """Return (source_distance, ray_model, is_infinity)."""
    payload: dict | None = None
    params: dict = {}
    metadata = run_dir / "run_metadata.json"
    if metadata.is_file():
        payload = json.loads(metadata.read_text(encoding="utf-8"))
        params = dict(payload.get("effective_parameters", {}))

    ray_model = str(params.get("ray-model", "point"))
    is_infinity = detect_infinity_run(run_dir, payload, params)
    if is_infinity:
        return math.inf, "parallel", True

    if "source-distance" in params:
        return float(params["source-distance"]), ray_model, False
    if payload and "sweep_value" in payload and not isinstance(payload["sweep_value"], str):
        return float(payload["sweep_value"]), ray_model, False

    summary = run_dir / "run_summary.txt"
    if summary.is_file():
        summary_map = {}
        for line in summary.read_text(encoding="utf-8").splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                summary_map[key] = value
        if "source_distance" in summary_map:
            if "ray_model" in summary_map:
                ray_model = summary_map["ray_model"]
            return float(summary_map["source_distance"]), ray_model, False

    name = run_dir.name.replace("p", ".").replace("m", "-")
    try:
        return float(name), ray_model, False
    except ValueError as exc:
        raise SystemExit(f"Cannot determine source distance for {run_dir}") from exc


def load_image_csv(path: Path) -> tuple[np.ndarray, dict[str, float]]:
    meta: dict[str, float] = {}
    rows: list[list[float]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        if line.startswith("#"):
            body = line[1:].strip()
            key, _, value = body.partition(",")
            if key in {"width", "height", "u_min", "u_max", "v_min", "v_max"}:
                meta[key] = float(value)
            continue
        rows.append([float(x) for x in line.split(",")])

    intensity = np.asarray(rows, dtype=float)
    required = {"u_min", "u_max", "v_min", "v_max"}
    missing = required - set(meta)
    if missing:
        raise SystemExit(f"{path}: missing metadata keys {sorted(missing)}")
    if intensity.ndim != 2:
        raise SystemExit(f"{path}: expected 2D intensity grid")
    return intensity, meta


def load_sweep_runs(sweep_dir: Path) -> list[SweepRun]:
    runs: list[SweepRun] = []
    for child in sorted(sweep_dir.iterdir()):
        if not child.is_dir() or child.name == "plots":
            continue
        csv_path = child / "einstein_ring.csv"
        if not csv_path.is_file():
            continue
        intensity, meta = load_image_csv(csv_path)
        source_distance, ray_model, is_infinity = parse_run_identity(child)
        runs.append(
            SweepRun(
                source_distance=source_distance,
                directory=child,
                intensity=intensity,
                u_min=meta["u_min"],
                u_max=meta["u_max"],
                v_min=meta["v_min"],
                v_max=meta["v_max"],
                ray_model=ray_model,
                is_infinity=is_infinity,
            )
        )
    if not runs:
        raise SystemExit(f"No einstein_ring.csv runs found in {sweep_dir}")

    finite = [run for run in runs if not run.is_infinity]
    infinite = [run for run in runs if run.is_infinity]
    finite.sort(key=lambda run: run.source_distance)
    return finite + infinite


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------


def pixel_radius_grid(run: SweepRun) -> np.ndarray:
    height, width = run.intensity.shape
    du = (run.u_max - run.u_min) / width
    dv = (run.v_max - run.v_min) / height
    xs = np.arange(width, dtype=float)
    ys = np.arange(height, dtype=float)
    u = run.u_min + (xs + 0.5) * du
    v = run.v_min + (ys + 0.5) * dv
    uu, vv = np.meshgrid(u, v)
    return np.hypot(uu, vv)


def radial_intensity_profile(
    run: SweepRun, n_bins: int = N_RADIUS_BINS
) -> tuple[np.ndarray, np.ndarray]:
    """Return (bin_center_radius, mean_intensity) using pixel-center radii."""
    radii = pixel_radius_grid(run)
    intensity = run.intensity
    r_max = max(abs(run.u_min), abs(run.u_max), abs(run.v_min), abs(run.v_max))
    edges = np.linspace(0.0, r_max, n_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])

    flat_r = radii.ravel()
    flat_i = intensity.ravel()
    sums, _ = np.histogram(flat_r, bins=edges, weights=flat_i)
    counts, _ = np.histogram(flat_r, bins=edges)
    with np.errstate(invalid="ignore", divide="ignore"):
        mean = np.divide(sums, counts, out=np.zeros_like(sums), where=counts > 0)
    return centers, mean


def ring_radius(run: SweepRun) -> float:
    """Radius of peak mean radial intensity (primary ring radius estimator)."""
    centers, mean = radial_intensity_profile(run)
    if not np.any(mean > 0):
        return float("nan")
    return float(centers[int(np.argmax(mean))])


def intensity_weighted_radius(run: SweepRun) -> float:
    radii = pixel_radius_grid(run)
    weights = run.intensity
    total = float(weights.sum())
    if total <= 0.0:
        return float("nan")
    return float((radii * weights).sum() / total)


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------


def plot_radius_vs_distance(runs: list[SweepRun], out_path: Path) -> list[dict[str, object]]:
    finite = [run for run in runs if not run.is_infinity]
    infinite = [run for run in runs if run.is_infinity]

    distances = [run.source_distance for run in finite]
    peak_radii = [ring_radius(run) for run in finite]
    mean_radii = [intensity_weighted_radius(run) for run in finite]

    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    if distances:
        ax.plot(distances, peak_radii, "o-", label="peak radial intensity")
        ax.plot(distances, mean_radii, "s--", label="intensity-weighted mean radius")

    for run in infinite:
        peak = ring_radius(run)
        mean = intensity_weighted_radius(run)
        ax.axhline(peak, color="C3", linestyle=":", linewidth=1.8, label=r"$S=\infty$ peak radius")
        ax.axhline(
            mean,
            color="C3",
            linestyle="--",
            linewidth=1.2,
            alpha=0.8,
            label=r"$S=\infty$ weighted-mean radius",
        )
        if distances:
            x_marker = max(distances) * 1.05
            ax.scatter([x_marker], [peak], color="C3", marker="*", s=140, zorder=5)
            ax.scatter([x_marker], [mean], color="C3", marker="D", s=45, zorder=5)

    ax.set_xlabel("source distance")
    ax.set_ylabel("ring radius (image-plane units)")
    ax.set_title("Einstein ring radius vs source distance")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)

    rows: list[dict[str, object]] = []
    for run, peak, mean in zip(finite, peak_radii, mean_radii):
        rows.append(
            {
                "source_distance": run.source_distance,
                "source_distance_label": f"{run.source_distance:g}",
                "ray_model": run.ray_model,
                "ring_radius_peak": peak,
                "ring_radius_weighted_mean": mean,
            }
        )
    for run in infinite:
        rows.append(
            {
                "source_distance": "inf",
                "source_distance_label": "inf",
                "ray_model": "parallel",
                "ring_radius_peak": ring_radius(run),
                "ring_radius_weighted_mean": intensity_weighted_radius(run),
            }
        )
    return rows


def plot_radial_profiles(runs: list[SweepRun], out_path: Path) -> None:
    fig, ax = plt.subplots(figsize=(7.5, 4.8))
    for run in runs:
        centers, mean = radial_intensity_profile(run)
        style = {"linewidth": 2.2, "linestyle": "--"} if run.is_infinity else {}
        ax.plot(centers, mean, label=run.label, **style)
    ax.set_xlabel("radius from image center")
    ax.set_ylabel("mean intensity in radial bin")
    ax.set_title("Radial intensity distribution (all source distances)")
    ax.grid(True, alpha=0.3)
    ax.legend(title="source distance")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def write_radius_csv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "source_distance",
                "source_distance_label",
                "ray_model",
                "ring_radius_peak",
                "ring_radius_weighted_mean",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    root = repo_root()
    sweep_root = root / SWEEP_ROOT
    sweep_dir = find_latest_source_distance_sweep(sweep_root)
    runs = load_sweep_runs(sweep_dir)

    out_dir = sweep_dir / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)

    radius_png = out_dir / "ring_radius_vs_source_distance.png"
    profiles_png = out_dir / "radial_intensity_profiles.png"
    radius_csv = out_dir / "ring_radius_vs_source_distance.csv"

    rows = plot_radius_vs_distance(runs, radius_png)
    plot_radial_profiles(runs, profiles_png)
    write_radius_csv(radius_csv, rows)

    print(f"Loaded sweep: {sweep_dir}")
    print(f"Runs: {[run.label for run in runs]}")
    print("Ring radii (peak):")
    for row in rows:
        print(
            f"  S={row['source_distance_label']}  "
            f"model={row['ray_model']}  "
            f"R_peak={float(row['ring_radius_peak']):.6g}  "
            f"R_mean={float(row['ring_radius_weighted_mean']):.6g}"
        )
    print(f"Wrote {radius_png}")
    print(f"Wrote {profiles_png}")
    print(f"Wrote {radius_csv}")

    if any(math.isnan(float(row["ring_radius_peak"])) for row in rows):
        raise SystemExit("One or more runs have empty images (NaN ring radius).")
    if not any(run.is_infinity for run in runs):
        print(
            "WARNING: no S=∞ (parallel) run found. "
            "Add 'inf' to SWEEP_VALUES and re-run parameter_sweep.py."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
