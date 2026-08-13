#!/usr/bin/env python3
"""Test 3 — RK4 step-size convergence of direct theta_E.

Fixes one finite source distance and varies the integration step size.
theta_E is read from the observer-hit root, not the image.
"""

from __future__ import annotations

import time

import matplotlib.pyplot as plt

from _common import results_dir, run_canonical, save_figure, write_csv, write_json

# ---------------------------------------------------------------------------
# Configuration (edit here)
# ---------------------------------------------------------------------------

TEST_NAME = "step_convergence_test"

SOURCE_DISTANCE = 100.0
STEP_SIZES = [0.02, 0.01, 0.005]

BASE_PARAMS = {
    "ray-count": 41,
    "azimuth-count": 64,
    "resolution": 64,
    "extent": 0.8,
    "b-min": 2.0,
    "b-max": 20.0,
    "step-size": 0.01,  # overwritten per run
    "max-steps": 600000,  # allow finer steps enough budget
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
    for step in STEP_SIZES:
        params = dict(BASE_PARAMS)
        params["step-size"] = step
        dirname = f"step_{str(step).replace('.', 'p')}"
        result = run_canonical(
            run_dir=out / dirname,
            params=params,
            label=f"step_size={step} S={SOURCE_DISTANCE:g}",
        )
        rows.append(
            {
                "step_size": step,
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

    # Finest step is the last entry (smallest h)
    reference = float(rows[-1]["theta_E"])
    for row in rows:
        row["theta_E_ref"] = reference
        row["relative_error"] = (float(row["theta_E"]) - reference) / reference

    write_csv(
        out / "results.csv",
        rows,
        [
            "step_size",
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
            "STEP_SIZES": STEP_SIZES,
            "SOURCE_DISTANCE": SOURCE_DISTANCE,
            "reference_step_size": STEP_SIZES[-1],
            "reference_theta_E": reference,
            "total_elapsed_seconds": total_elapsed,
            "rows": rows,
        },
    )

    steps = [float(r["step_size"]) for r in rows]
    thetas = [float(r["theta_E"]) for r in rows]
    rel_err = [float(r["relative_error"]) for r in rows]

    plt.figure(figsize=(7.0, 4.4))
    plt.plot(steps, thetas, "o-")
    plt.xlabel("RK4 step size")
    plt.ylabel(r"$\theta_E$ (rad)")
    plt.title(rf"Step-size convergence of $\theta_E$ (S={SOURCE_DISTANCE:g})")
    plt.gca().invert_xaxis()
    plt.grid(True, alpha=0.3)
    save_figure(out / "theta_E_vs_step_size.png")

    plt.figure(figsize=(7.0, 4.4))
    plt.plot(steps, rel_err, "s-")
    plt.axhline(0.0, color="0.5", linestyle="--", linewidth=1.0)
    plt.xlabel("RK4 step size")
    plt.ylabel(rf"$(\theta_E - \theta_E^{{h={STEP_SIZES[-1]}}})/\theta_E^{{h={STEP_SIZES[-1]}}}$")
    plt.title("Relative error vs finest step size")
    plt.gca().invert_xaxis()
    plt.grid(True, alpha=0.3)
    save_figure(out / "theta_E_relative_error_vs_step_size.png")

    abs_errs = [abs(float(r["relative_error"])) for r in rows]
    # As step decreases (left to right in STEP_SIZES), |error| vs finest should shrink
    shrinking = all(abs_errs[i + 1] <= abs_errs[i] + 1e-12 for i in range(len(abs_errs) - 1))
    write_json(
        out / "verdict.json",
        {
            "absolute_relative_error_shrinks_with_finer_step": shrinking,
            "theta_E_values": {str(r["step_size"]): r["theta_E"] for r in rows},
            "relative_errors": {str(r["step_size"]): r["relative_error"] for r in rows},
            "total_elapsed_seconds": total_elapsed,
        },
    )

    print("\n=== Step-convergence test summary ===")
    for row in rows:
        print(
            f"  h={float(row['step_size']):.4g}  "
            f"theta_E={float(row['theta_E']):.8g}  "
            f"rel_err={float(row['relative_error']):.3e}"
        )
    print(f"|rel_err| shrinks with finer step: {shrinking}")
    print(f"total runtime: {total_elapsed:.1f}s")
    print(f"results: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
