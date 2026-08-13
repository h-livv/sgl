#!/usr/bin/env python3
"""Test 1 — Main source-distance experiment (direct theta_E observable).

Measures the observer-hit Einstein-ring angle theta_E(S) for a reduced
source-distance sweep, including the parallel-ray (S=infinity) limit.

Parameters are defined below. Edit them to change the exploratory sweep.
"""

from __future__ import annotations

import math
import time
from pathlib import Path

import matplotlib.pyplot as plt

from _common import (
    results_dir,
    run_canonical,
    save_figure,
    write_csv,
    write_json,
)

# ---------------------------------------------------------------------------
# Configuration (edit here)
# ---------------------------------------------------------------------------

TEST_NAME = "source_distance_test"

# Reduced exploratory source distances. Use "inf" for parallel rays.
SOURCE_DISTANCES = [20, 50, 100, 200, 1000, "inf"]

# Shared pipeline parameters. Image resolution/azimuth are visualization-only;
# theta_E comes from the observer-hit root, not from the image.
BASE_PARAMS = {
    "ray-count": 41,
    "azimuth-count": 64,
    "resolution": 64,
    "extent": 0.8,
    "b-min": 2.0,
    "b-max": 20.0,
    "step-size": 0.01,
    "max-steps": 300000,
    "observer-axial-distance": 30.0,
    "observer-distance": 0.0,
    "observer-hit-tolerance": 1e-6,
    "max-root-iterations": 60,
    "source-distance": 30.0,  # launch-plane distance for parallel/inf case
    "ray-model": "point",
}

# ---------------------------------------------------------------------------
# Experiment
# ---------------------------------------------------------------------------


def prepare_params(source_distance):
    params = dict(BASE_PARAMS)
    if source_distance == "inf" or (
        isinstance(source_distance, float) and math.isinf(source_distance)
    ):
        params["ray-model"] = "parallel"
        dirname = "inf"
        label_S = math.inf
    else:
        params["source-distance"] = float(source_distance)
        params["ray-model"] = "point"
        dirname = str(source_distance).replace(".", "p")
        label_S = float(source_distance)
    return params, dirname, label_S


def main() -> int:
    out = results_dir(TEST_NAME)
    t_start = time.perf_counter()

    rows = []
    for S in SOURCE_DISTANCES:
        params, dirname, label_S = prepare_params(S)
        result = run_canonical(
            run_dir=out / dirname,
            params=params,
            label=f"source_distance S={S}",
        )
        rows.append(
            {
                "source_distance": "inf" if math.isinf(label_S) else label_S,
                "source_distance_label": "inf" if math.isinf(label_S) else f"{label_S:g}",
                "is_infinity": math.isinf(label_S),
                "ray_model": params["ray-model"],
                "theta_E": result["theta_E"],
                "rho": result["rho"],
                "R_equiv": result["R_equiv"],
                "selected_observer_hit_b": result["selected_observer_hit_b"],
                "selected_observer_hit_residual_u": result["selected_observer_hit_residual_u"],
                "selected_angular_u": result["selected_angular_u"],
                "selected_angular_v": result["selected_angular_v"],
                "observer_hit_candidate_count": result["observer_hit_candidate_count"],
                "elapsed_seconds": result["elapsed_seconds"],
                "output_dir": result["output_dir"],
            }
        )

    finite = [r for r in rows if not r["is_infinity"]]
    infinite = [r for r in rows if r["is_infinity"]]
    if not infinite:
        raise SystemExit("No S=inf run; cannot form relative deviation.")
    theta_inf = float(infinite[0]["theta_E"])
    for row in finite:
        row["theta_relative"] = (float(row["theta_E"]) - theta_inf) / theta_inf
    for row in infinite:
        row["theta_relative"] = 0.0

    write_csv(
        out / "results.csv",
        rows,
        [
            "source_distance",
            "source_distance_label",
            "is_infinity",
            "ray_model",
            "theta_E",
            "rho",
            "R_equiv",
            "theta_relative",
            "selected_observer_hit_b",
            "selected_observer_hit_residual_u",
            "selected_angular_u",
            "selected_angular_v",
            "observer_hit_candidate_count",
            "elapsed_seconds",
            "output_dir",
        ],
    )

    total_elapsed = time.perf_counter() - t_start
    metadata = {
        "test": TEST_NAME,
        "BASE_PARAMS": BASE_PARAMS,
        "SOURCE_DISTANCES": SOURCE_DISTANCES,
        "theta_E_infinity": theta_inf,
        "total_elapsed_seconds": total_elapsed,
        "observable": "theta_E from observer-hit root (not image peak)",
        "rows": rows,
    }
    write_json(out / "metadata.json", metadata)

    # Plot 1: theta_E vs S
    distances = [float(r["source_distance"]) for r in finite]
    thetas = [float(r["theta_E"]) for r in finite]
    plt.figure(figsize=(7.0, 4.4))
    plt.plot(distances, thetas, "o-", label=r"$\theta_E(S)$")
    plt.axhline(theta_inf, color="C3", linestyle=":", label=r"$\theta_E(\infty)$")
    if distances:
        plt.scatter([max(distances) * 1.05], [theta_inf], color="C3", marker="*", s=120)
    plt.xlabel("source distance S")
    plt.ylabel(r"$\theta_E$ (rad)")
    plt.title(r"Direct $\theta_E$ vs source distance")
    plt.grid(True, alpha=0.3)
    plt.legend()
    save_figure(out / "theta_E_vs_source_distance.png")

    # Plot 2: relative deviation from infinity
    relatives = [float(r["theta_relative"]) for r in finite]
    plt.figure(figsize=(7.0, 4.4))
    plt.plot(distances, relatives, "o-", label=r"$(\theta_E(S)-\theta_E(\infty))/\theta_E(\infty)$")
    plt.axhline(0.0, color="0.5", linestyle="--", linewidth=1.0)
    plt.xlabel("source distance S")
    plt.ylabel(r"$(\theta_E(S)-\theta_E(\infty))/\theta_E(\infty)$")
    plt.title("Relative deviation from parallel-ray limit")
    plt.grid(True, alpha=0.3)
    plt.legend()
    save_figure(out / "theta_E_relative_vs_source_distance.png")

    # Qualitative checks
    increasing = all(
        float(finite[i + 1]["theta_E"]) > float(finite[i]["theta_E"])
        for i in range(len(finite) - 1)
    )
    approaches = abs(float(finite[-1]["theta_E"]) - theta_inf) <= abs(
        float(finite[0]["theta_E"]) - theta_inf
    )
    write_json(
        out / "verdict.json",
        {
            "theta_E_increases_with_S": increasing,
            "finite_approaches_infinity": approaches,
            "theta_E_values": {
                r["source_distance_label"]: r["theta_E"] for r in rows
            },
            "total_elapsed_seconds": total_elapsed,
        },
    )

    print("\n=== Source-distance test summary ===")
    for row in rows:
        print(
            f"  S={row['source_distance_label']:>6}  "
            f"theta_E={float(row['theta_E']):.8g}  "
            f"rel={float(row['theta_relative']):.6g}  "
            f"b={float(row['selected_observer_hit_b']):.6g}"
        )
    print(f"increasing with S: {increasing}")
    print(f"approaches infinity: {approaches}")
    print(f"total runtime: {total_elapsed:.1f}s")
    print(f"results: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
