#!/usr/bin/env python3
"""Test 2 — Ray-sampling convergence of direct theta_E.

Fixes one finite source distance and varies ray-count. All other parameters
stay fixed. theta_E is read from the observer-hit root, not the image.
"""

from __future__ import annotations

import time

import matplotlib.pyplot as plt

from _common import results_dir, run_canonical, save_figure, write_csv, write_json

# ---------------------------------------------------------------------------
# Configuration (edit here)
# ---------------------------------------------------------------------------

TEST_NAME = "ray_convergence_test"

SOURCE_DISTANCE = 100.0
RAY_COUNTS = [101, 201, 401]

BASE_PARAMS = {
    "ray-count": 101,  # overwritten per run
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
    "source-distance": SOURCE_DISTANCE,
    "ray-model": "point",
}

# ---------------------------------------------------------------------------
# Experiment
# ---------------------------------------------------------------------------


def main() -> int:
    out = results_dir(TEST_NAME)
    t_start = time.perf_counter()

    rows = []
    for ray_count in RAY_COUNTS:
        params = dict(BASE_PARAMS)
        params["ray-count"] = ray_count
        result = run_canonical(
            run_dir=out / f"rays_{ray_count}",
            params=params,
            label=f"ray_count={ray_count} S={SOURCE_DISTANCE:g}",
        )
        rows.append(
            {
                "ray_count": ray_count,
                "source_distance": SOURCE_DISTANCE,
                "theta_E": result["theta_E"],
                "rho": result["rho"],
                "R_equiv": result["R_equiv"],
                "selected_observer_hit_b": result["selected_observer_hit_b"],
                "selected_observer_hit_residual_u": result["selected_observer_hit_residual_u"],
                "elapsed_seconds": result["elapsed_seconds"],
                "output_dir": result["output_dir"],
            }
        )

    reference = float(rows[-1]["theta_E"])
    for row in rows:
        row["theta_E_ref"] = reference
        row["relative_error"] = (float(row["theta_E"]) - reference) / reference

    write_csv(
        out / "results.csv",
        rows,
        [
            "ray_count",
            "source_distance",
            "theta_E",
            "rho",
            "R_equiv",
            "theta_E_ref",
            "relative_error",
            "selected_observer_hit_b",
            "selected_observer_hit_residual_u",
            "elapsed_seconds",
            "output_dir",
        ],
    )

    total_elapsed = time.perf_counter() - t_start
    write_json(
        out / "metadata.json",
        {
            "test": TEST_NAME,
            "BASE_PARAMS": BASE_PARAMS,
            "RAY_COUNTS": RAY_COUNTS,
            "SOURCE_DISTANCE": SOURCE_DISTANCE,
            "reference_ray_count": RAY_COUNTS[-1],
            "reference_theta_E": reference,
            "total_elapsed_seconds": total_elapsed,
            "rows": rows,
        },
    )

    counts = [int(r["ray_count"]) for r in rows]
    thetas = [float(r["theta_E"]) for r in rows]
    rel_err = [float(r["relative_error"]) for r in rows]

    plt.figure(figsize=(7.0, 4.4))
    plt.plot(counts, thetas, "o-")
    plt.xlabel("ray count")
    plt.ylabel(r"$\theta_E$ (rad)")
    plt.title(rf"Ray-count convergence of $\theta_E$ (S={SOURCE_DISTANCE:g})")
    plt.grid(True, alpha=0.3)
    save_figure(out / "theta_E_vs_ray_count.png")

    plt.figure(figsize=(7.0, 4.4))
    plt.plot(counts, rel_err, "s-")
    plt.axhline(0.0, color="0.5", linestyle="--", linewidth=1.0)
    plt.xlabel("ray count")
    plt.ylabel(rf"$(\theta_E - \theta_E^{{N={RAY_COUNTS[-1]}}})/\theta_E^{{N={RAY_COUNTS[-1]}}}$")
    plt.title("Relative error vs highest ray count")
    plt.grid(True, alpha=0.3)
    save_figure(out / "theta_E_relative_error_vs_ray_count.png")

    # Sensible convergence: absolute relative errors should not grow with N
    abs_errs = [abs(float(r["relative_error"])) for r in rows]
    nonincreasing = all(abs_errs[i + 1] <= abs_errs[i] + 1e-12 for i in range(len(abs_errs) - 1))
    write_json(
        out / "verdict.json",
        {
            "absolute_relative_error_nonincreasing": nonincreasing,
            "theta_E_values": {str(r["ray_count"]): r["theta_E"] for r in rows},
            "relative_errors": {str(r["ray_count"]): r["relative_error"] for r in rows},
            "total_elapsed_seconds": total_elapsed,
        },
    )

    print("\n=== Ray-convergence test summary ===")
    for row in rows:
        print(
            f"  N={int(row['ray_count']):4d}  "
            f"theta_E={float(row['theta_E']):.8g}  "
            f"rel_err={float(row['relative_error']):.3e}"
        )
    print(f"|rel_err| nonincreasing: {nonincreasing}")
    print(f"total runtime: {total_elapsed:.1f}s")
    print(f"results: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
