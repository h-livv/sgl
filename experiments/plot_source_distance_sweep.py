#!/usr/bin/env python3
"""Plot diagnostics from the latest source_distance sweep.

Produces:
  1. Ring radius vs source distance (rho, theta, R_equiv from run_summary)
  2. Relative angular deviation (theta(S) - theta_inf) / theta_inf vs S
  3. Radial intensity profiles (fraction of total pixel counts per rho bin)

Radius measurements use ``selected_angular_radius`` from each run's
``run_summary.txt`` — the refined observer-hit gnomonic coordinate
rho = sqrt(u_ang^2 + v_ang^2) written by the canonical executable before
azimuthal expansion or pixel binning. Image-derived radii are retained only
as cross-checks.

Intensity profiles denormalize the CSV grid using ``raw_image_max`` from the
same summary, then plot the fraction of total counts in each radial bin so
curves are comparable across source distances.

Usage (from repository root):

    python3 experiments/plot_source_distance_sweep.py
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass, field
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SWEEP_ROOT = Path("outputs/sweeps")
SWEEP_NAME = "source_distance"
N_RADIUS_BINS = 256
INFINITY_DIRNAME = "inf"


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclass
class RunSummary:
    """Parsed run_summary.txt fields relevant to angular measurements."""

    image_observable: str = ""
    observer_axial_distance: float = 30.0
    selected_angular_radius: float | None = None
    selected_angular_u: float | None = None
    selected_angular_v: float | None = None
    raw_image_max: float = 1.0
    angular_samples: int = 0
    observer_hit_count: int = 0
    extra: dict[str, str] = field(default_factory=dict)


@dataclass
class SweepRun:
    source_distance: float  # math.inf for parallel / source-at-infinity
    directory: Path
    intensity: np.ndarray  # normalized CSV grid, shape (height, width)
    u_min: float
    u_max: float
    v_min: float
    v_max: float
    summary: RunSummary
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

    @property
    def denormalized_intensity(self) -> np.ndarray:
        """Restore raw pixel counts from max-normalized CSV values."""
        return self.intensity * self.summary.raw_image_max


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def find_latest_source_distance_sweep(root: Path) -> Path:
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


def parse_summary_file(path: Path) -> RunSummary:
    summary = RunSummary()
    if not path.is_file():
        return summary

    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        summary.extra[key] = value

    def maybe_float(key: str) -> float | None:
        text = summary.extra.get(key)
        if text is None:
            return None
        return float(text)

    summary.image_observable = summary.extra.get("image_observable", "")
    summary.observer_axial_distance = float(summary.extra.get("observer_axial_distance", "30"))
    summary.selected_angular_radius = maybe_float("selected_angular_radius")
    summary.selected_angular_u = maybe_float("selected_angular_u")
    summary.selected_angular_v = maybe_float("selected_angular_v")
    summary.raw_image_max = float(summary.extra.get("raw_image_max", "1"))
    summary.angular_samples = int(float(summary.extra.get("angular_samples", "0")))
    summary.observer_hit_count = int(float(summary.extra.get("observer_hit_count", "0")))
    return summary


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
    if model == "parallel" and run_dir.parent.name == SWEEP_NAME:
        return run_dir.name.lower() in {INFINITY_DIRNAME, "infinity"} or bool(
            payload and payload.get("source_at_infinity")
        )
    return False


def parse_run_identity(run_dir: Path) -> tuple[float, str, bool]:
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

    summary = parse_summary_file(run_dir / "run_summary.txt")
    if "source_distance" in summary.extra:
        if "ray_model" in summary.extra:
            ray_model = summary.extra["ray_model"]
        return float(summary.extra["source_distance"]), ray_model, False

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
            if key in {"width", "height", "u_min", "u_max", "v_min", "v_max", "raw_image_max"}:
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
        summary = parse_summary_file(child / "run_summary.txt")
        if summary.raw_image_max == 1.0 and "raw_image_max" in meta:
            summary.raw_image_max = meta["raw_image_max"]
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
                summary=summary,
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
# Physics-aligned measurements
# ---------------------------------------------------------------------------


def rho_from_summary(run: SweepRun) -> float:
    """Gnomonic tangent-plane radius rho = sqrt(u_ang^2 + v_ang^2).

    Primary source: selected_angular_radius from the observer-hit refinement
    stage. Falls back to hypot(u,v) from summary components, then image centroid.
    """
    rho = run.summary.selected_angular_radius
    if rho is not None and math.isfinite(rho) and rho > 0.0:
        return rho

    u = run.summary.selected_angular_u
    v = run.summary.selected_angular_v
    if u is not None and v is not None and math.isfinite(u) and math.isfinite(v):
        candidate = math.hypot(u, v)
        if candidate > 0.0:
            return candidate

    return image_intensity_weighted_rho(run)


def theta_from_rho(rho: float) -> float:
    """True angular radius theta = atan(rho) for gnomonic coordinates."""
    return math.atan(rho)


def equivalent_ring_radius(run: SweepRun, rho: float) -> float:
    """Screen-equivalent radius R_equiv = D * rho (gnomonic exact)."""
    return run.summary.observer_axial_distance * rho


def pixel_rho_grid(run: SweepRun) -> np.ndarray:
    height, width = run.intensity.shape
    du = (run.u_max - run.u_min) / width
    dv = (run.v_max - run.v_min) / height
    xs = np.arange(width, dtype=float)
    ys = np.arange(height, dtype=float)
    u = run.u_min + (xs + 0.5) * du
    v = run.v_min + (ys + 0.5) * dv
    uu, vv = np.meshgrid(u, v)
    return np.hypot(uu, vv)


def radial_fraction_profile(
    run: SweepRun, n_bins: int = N_RADIUS_BINS
) -> tuple[np.ndarray, np.ndarray]:
    """Return (bin_center_rho, fraction_of_total_counts_in_bin).

    Uses denormalized pixel counts so the profile reflects actual binning
    weights, then normalizes each curve to unit total area under the histogram.
    """
    rho = pixel_rho_grid(run)
    counts = run.denormalized_intensity
    total = float(counts.sum())
    if total <= 0.0:
        return np.zeros(n_bins), np.zeros(n_bins)

    r_max = max(abs(run.u_min), abs(run.u_max), abs(run.v_min), abs(run.v_max))
    edges = np.linspace(0.0, r_max, n_bins + 1)
    centers = 0.5 * (edges[:-1] + edges[1:])

    sums, _ = np.histogram(rho.ravel(), bins=edges, weights=counts.ravel())
    return centers, sums / total


def image_peak_rho(run: SweepRun) -> float:
    """Cross-check: rho at peak of the radial fraction profile."""
    centers, fraction = radial_fraction_profile(run)
    if not np.any(fraction > 0):
        return float("nan")
    return float(centers[int(np.argmax(fraction))])


def image_intensity_weighted_rho(run: SweepRun) -> float:
    """Cross-check: intensity-weighted mean rho from denormalized pixels."""
    rho = pixel_rho_grid(run)
    weights = run.denormalized_intensity
    total = float(weights.sum())
    if total <= 0.0:
        return float("nan")
    return float((rho * weights).sum() / total)


def measure_run(run: SweepRun) -> dict[str, float]:
    rho = rho_from_summary(run)
    theta = theta_from_rho(rho)
    return {
        "rho": rho,
        "theta": theta,
        "r_equiv": equivalent_ring_radius(run, rho),
        "rho_image_peak": image_peak_rho(run),
        "rho_image_weighted": image_intensity_weighted_rho(run),
    }


def row_from_run(run: SweepRun, theta_inf: float | None) -> dict[str, object]:
    metrics = measure_run(run)
    theta_relative: float | str
    if run.is_infinity or theta_inf is None or theta_inf <= 0.0:
        theta_relative = ""
    else:
        theta_relative = (metrics["theta"] - theta_inf) / theta_inf

    return {
        "source_distance": "inf" if run.is_infinity else run.source_distance,
        "source_distance_label": "inf" if run.is_infinity else f"{run.source_distance:g}",
        "ray_model": run.ray_model,
        "rho": metrics["rho"],
        "theta": metrics["theta"],
        "r_equiv": metrics["r_equiv"],
        "theta_relative": theta_relative,
        "rho_image_peak": metrics["rho_image_peak"],
        "rho_image_weighted": metrics["rho_image_weighted"],
        "raw_image_max": run.summary.raw_image_max,
        "angular_samples": run.summary.angular_samples,
    }


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------


def plot_radius_vs_distance(
    finite: list[SweepRun], infinite: list[SweepRun], out_path: Path
) -> None:
    distances = [run.source_distance for run in finite]
    rho_vals = [measure_run(run)["rho"] for run in finite]
    theta_vals = [measure_run(run)["theta"] for run in finite]
    r_equiv_vals = [measure_run(run)["r_equiv"] for run in finite]

    fig, (ax_rho, ax_theta) = plt.subplots(1, 2, figsize=(11.0, 4.6))

    if distances:
        ax_rho.plot(distances, rho_vals, "o-", label=r"$\rho$ from summary")
        ax_theta.plot(distances, theta_vals, "o-", label=r"$\theta=\arctan(\rho)$")

    for run in infinite:
        metrics = measure_run(run)
        ax_rho.axhline(
            metrics["rho"],
            color="C3",
            linestyle=":",
            linewidth=1.8,
            label=r"$S=\infty$ $\rho$",
        )
        ax_theta.axhline(
            metrics["theta"],
            color="C3",
            linestyle=":",
            linewidth=1.8,
            label=r"$S=\infty$ $\theta$",
        )
        if distances:
            x_marker = max(distances) * 1.05
            ax_rho.scatter([x_marker], [metrics["rho"]], color="C3", marker="*", s=120, zorder=5)
            ax_theta.scatter([x_marker], [metrics["theta"]], color="C3", marker="*", s=120, zorder=5)

    ax_rho.set_xlabel("source distance S")
    ax_rho.set_ylabel(r"gnomonic radius $\rho$")
    ax_rho.set_title(r"$\rho$ vs source distance")
    ax_rho.grid(True, alpha=0.3)
    ax_rho.legend()

    ax_theta.set_xlabel("source distance S")
    ax_theta.set_ylabel(r"angular radius $\theta$ (rad)")
    ax_theta.set_title(r"$\theta$ vs source distance")
    ax_theta.grid(True, alpha=0.3)
    ax_theta.legend()

    fig.suptitle(
        r"Einstein ring radius from observer-hit summary ($R_{\mathrm{equiv}}=D\rho$)",
        y=1.02,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=160, bbox_inches="tight")
    plt.close(fig)

    # Secondary plot for R_equiv alone (useful for weak-lens comparison)
    fig2, ax = plt.subplots(figsize=(7.2, 4.6))
    if distances:
        ax.plot(distances, r_equiv_vals, "o-", label=r"$R_{\mathrm{equiv}} = D\rho$")
    for run in infinite:
        metrics = measure_run(run)
        ax.axhline(
            metrics["r_equiv"],
            color="C3",
            linestyle=":",
            linewidth=1.8,
            label=r"$S=\infty$ $R_{\mathrm{equiv}}$",
        )
        if distances:
            ax.scatter([max(distances) * 1.05], [metrics["r_equiv"]], color="C3", marker="*", s=120)
    ax.set_xlabel("source distance S")
    ax.set_ylabel(r"$R_{\mathrm{equiv}} = D\rho$")
    ax.set_title("Equivalent screen radius vs source distance")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig2.tight_layout()
    r_equiv_path = out_path.with_name("ring_r_equiv_vs_source_distance.png")
    fig2.savefig(r_equiv_path, dpi=160)
    plt.close(fig2)


def plot_theta_relative_vs_distance(
    finite: list[SweepRun], theta_inf: float, out_path: Path
) -> None:
    if theta_inf <= 0.0 or not math.isfinite(theta_inf):
        raise SystemExit("Cannot plot relative theta: invalid theta(inf).")

    distances = [run.source_distance for run in finite]
    relatives = [(measure_run(run)["theta"] - theta_inf) / theta_inf for run in finite]

    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    ax.plot(distances, relatives, "o-", label=r"$(\theta(S)-\theta_\infty)/\theta_\infty$")
    ax.axhline(0.0, color="0.5", linestyle="--", linewidth=1.0)
    ax.set_xlabel("source distance S")
    ax.set_ylabel(r"$(\theta(S)-\theta_\infty)/\theta_\infty$")
    ax.set_title("Relative angular deviation from parallel-ray limit")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def plot_radial_profiles(runs: list[SweepRun], out_path: Path) -> None:
    fig, ax = plt.subplots(figsize=(7.5, 4.8))
    for run in runs:
        centers, fraction = radial_fraction_profile(run)
        style = {"linewidth": 2.2, "linestyle": "--"} if run.is_infinity else {}
        ax.plot(centers, fraction, label=run.label, **style)
    ax.set_xlabel(r"gnomonic radius $\rho$")
    ax.set_ylabel("fraction of total pixel counts per radial bin")
    ax.set_title("Radial count distribution (denormalized, unit total)")
    ax.grid(True, alpha=0.3)
    ax.legend(title="source distance")
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def write_measurements_csv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "source_distance",
                "source_distance_label",
                "ray_model",
                "rho",
                "theta",
                "r_equiv",
                "theta_relative",
                "rho_image_peak",
                "rho_image_weighted",
                "raw_image_max",
                "angular_samples",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def warn_if_summary_missing(runs: list[SweepRun]) -> None:
    for run in runs:
        if run.summary.selected_angular_radius is None:
            print(
                f"WARNING: {run.directory.name}: no selected_angular_radius in summary; "
                "using image-derived fallback."
            )
        if run.summary.image_observable and run.summary.image_observable != "observer_angular_gnomonic":
            print(
                f"WARNING: {run.directory.name}: image_observable="
                f"{run.summary.image_observable!r} (expected observer_angular_gnomonic)."
            )


def main() -> int:
    root = repo_root()
    sweep_dir = find_latest_source_distance_sweep(root / SWEEP_ROOT)
    runs = load_sweep_runs(sweep_dir)
    warn_if_summary_missing(runs)

    finite = [run for run in runs if not run.is_infinity]
    infinite = [run for run in runs if run.is_infinity]

    theta_inf: float | None = None
    if infinite:
        theta_inf = measure_run(infinite[0])["theta"]

    rows = [row_from_run(run, theta_inf) for run in runs]

    out_dir = sweep_dir / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)

    radius_png = out_dir / "ring_radius_vs_source_distance.png"
    theta_rel_png = out_dir / "theta_relative_vs_source_distance.png"
    profiles_png = out_dir / "radial_intensity_profiles.png"
    measurements_csv = out_dir / "ring_measurements.csv"

    plot_radius_vs_distance(finite, infinite, radius_png)
    if theta_inf is not None:
        plot_theta_relative_vs_distance(finite, theta_inf, theta_rel_png)
    plot_radial_profiles(runs, profiles_png)
    write_measurements_csv(measurements_csv, rows)

    print(f"Loaded sweep: {sweep_dir}")
    print(f"Runs: {[run.label for run in runs]}")
    if theta_inf is not None:
        print(f"theta(inf) = {theta_inf:.6g} rad")
    print("Measurements (summary rho, theta, R_equiv):")
    for row in rows:
        rel = row["theta_relative"]
        rel_text = f"  rel={float(rel):.6g}" if rel != "" else ""
        print(
            f"  S={row['source_distance_label']}  "
            f"rho={float(row['rho']):.6g}  "
            f"theta={float(row['theta']):.6g}  "
            f"R_equiv={float(row['r_equiv']):.6g}"
            f"{rel_text}"
        )
    print(f"Wrote {radius_png}")
    if theta_inf is not None:
        print(f"Wrote {theta_rel_png}")
    print(f"Wrote {profiles_png}")
    print(f"Wrote {measurements_csv}")

    if any(math.isnan(float(row["rho"])) for row in rows):
        raise SystemExit("One or more runs have invalid rho measurements.")
    if not infinite:
        print(
            "WARNING: no S=inf (parallel) run found; relative-theta plot skipped. "
            "Add 'inf' to SWEEP_VALUES and re-run parameter_sweep.py."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
