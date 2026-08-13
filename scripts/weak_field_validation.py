#!/usr/bin/env python3
"""Test 4 — Weak-field analytical validation of direct theta_E.

Uses large observer/source distances so rs << D,S (with hard-coded rs=1 in the
canonical executable) and compares numerical theta_E to the weak-field formula:

    R_E = sqrt(2 rs D S / (D + S))          (finite S)
    R_E = sqrt(2 rs D)                      (S -> infinity)
    theta_analytic = atan(R_E / D)

Does not modify the physics model.
"""

from __future__ import annotations

import math
import time

import matplotlib.pyplot as plt

from _common import (
    results_dir,
    run_canonical,
    save_figure,
    weak_field_theta_E,
    write_csv,
    write_json,
)

# ---------------------------------------------------------------------------
# Configuration (edit here)
# ---------------------------------------------------------------------------

TEST_NAME = "weak_field_validation"

# Hard-coded in the C++ executable; documented here for the analytical reference.
RS = 1.0

# Large distances → weak-field regime (rs << D, S).
OBSERVER_AXIAL_DISTANCE = 1000.0
SOURCE_DISTANCES = [1000, 2000, 5000, "inf"]
LAUNCH_PLANE_FOR_INF = 1000.0  # parallel-ray launch plane distance

# Reduced exploratory numerics. Extent sized for expected rho ~ R_E/D ~ 0.03–0.05.
BASE_PARAMS = {
    "ray-count": 41,
    "azimuth-count": 64,
    "resolution": 64,
    "extent": 0.2,
    "b-min": 10.0,
    "b-max": 80.0,
    "step-size": 0.02,
    "max-steps": 300000,
    "observer-axial-distance": OBSERVER_AXIAL_DISTANCE,
    "observer-distance": 0.0,
    "observer-hit-tolerance": 1e-6,
    "max-root-iterations": 60,
    "source-distance": LAUNCH_PLANE_FOR_INF,
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
        params["source-distance"] = LAUNCH_PLANE_FOR_INF
        dirname = "inf"
        label_S = math.inf
        S_analytic = None
    else:
        params["source-distance"] = float(source_distance)
        params["ray-model"] = "point"
        dirname = str(source_distance).replace(".", "p")
        label_S = float(source_distance)
        S_analytic = label_S
    return params, dirname, label_S, S_analytic


def main() -> int:
    out = results_dir(TEST_NAME)
    t_start = time.perf_counter()

    rows = []
    for S in SOURCE_DISTANCES:
        params, dirname, label_S, S_analytic = prepare_params(S)
        result = run_canonical(
            run_dir=out / dirname,
            params=params,
            label=f"weak_field S={S} D={OBSERVER_AXIAL_DISTANCE:g}",
        )
        theta_num = float(result["theta_E"])
        theta_an = weak_field_theta_E(
            rs=RS, D=OBSERVER_AXIAL_DISTANCE, S=S_analytic
        )
        rel_err = (theta_num - theta_an) / theta_an
        rows.append(
            {
                "source_distance": "inf" if math.isinf(label_S) else label_S,
                "source_distance_label": "inf" if math.isinf(label_S) else f"{label_S:g}",
                "is_infinity": math.isinf(label_S),
                "observer_axial_distance": OBSERVER_AXIAL_DISTANCE,
                "rs": RS,
                "theta_E_numeric": theta_num,
                "theta_E_analytic": theta_an,
                "relative_error": rel_err,
                "rho": result["rho"],
                "R_equiv": result["R_equiv"],
                "selected_observer_hit_b": result["selected_observer_hit_b"],
                "selected_observer_hit_residual_u": result["selected_observer_hit_residual_u"],
                "elapsed_seconds": result["elapsed_seconds"],
                "output_dir": result["output_dir"],
            }
        )

    write_csv(
        out / "results.csv",
        rows,
        [
            "source_distance",
            "source_distance_label",
            "is_infinity",
            "observer_axial_distance",
            "rs",
            "theta_E_numeric",
            "theta_E_analytic",
            "relative_error",
            "rho",
            "R_equiv",
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
            "SOURCE_DISTANCES": SOURCE_DISTANCES,
            "RS": RS,
            "OBSERVER_AXIAL_DISTANCE": OBSERVER_AXIAL_DISTANCE,
            "total_elapsed_seconds": total_elapsed,
            "rows": rows,
        },
    )

    labels = [r["source_distance_label"] for r in rows]
    x = list(range(len(rows)))
    theta_num = [float(r["theta_E_numeric"]) for r in rows]
    theta_an = [float(r["theta_E_analytic"]) for r in rows]
    rel_err = [float(r["relative_error"]) for r in rows]

    plt.figure(figsize=(7.2, 4.6))
    plt.plot(x, theta_num, "o-", label=r"numerical $\theta_E$")
    plt.plot(x, theta_an, "s--", label=r"weak-field analytical $\theta_E$")
    plt.xticks(x, labels)
    plt.xlabel("source distance S")
    plt.ylabel(r"$\theta_E$ (rad)")
    plt.title(
        rf"Weak-field comparison (rs={RS:g}, D={OBSERVER_AXIAL_DISTANCE:g})"
    )
    plt.grid(True, alpha=0.3)
    plt.legend()
    save_figure(out / "theta_E_numeric_vs_analytic.png")

    plt.figure(figsize=(7.2, 4.6))
    plt.plot(x, rel_err, "o-")
    plt.axhline(0.0, color="0.5", linestyle="--", linewidth=1.0)
    plt.xticks(x, labels)
    plt.xlabel("source distance S")
    plt.ylabel(r"$(\theta_{\mathrm{num}}-\theta_{\mathrm{an}})/\theta_{\mathrm{an}}$")
    plt.title("Relative error vs weak-field prediction")
    plt.grid(True, alpha=0.3)
    save_figure(out / "theta_E_relative_error_vs_analytic.png")

    max_abs_err = max(abs(float(r["relative_error"])) for r in rows)
    # Exploratory: expect O(10%) or better at these distances; flag if wildly off.
    toward_prediction = max_abs_err < 0.5
    write_json(
        out / "verdict.json",
        {
            "max_abs_relative_error": max_abs_err,
            "within_50pct_of_analytic": toward_prediction,
            "theta_E_numeric": {
                r["source_distance_label"]: r["theta_E_numeric"] for r in rows
            },
            "theta_E_analytic": {
                r["source_distance_label"]: r["theta_E_analytic"] for r in rows
            },
            "relative_errors": {
                r["source_distance_label"]: r["relative_error"] for r in rows
            },
            "total_elapsed_seconds": total_elapsed,
        },
    )

    print("\n=== Weak-field validation summary ===")
    for row in rows:
        print(
            f"  S={row['source_distance_label']:>6}  "
            f"num={float(row['theta_E_numeric']):.8g}  "
            f"an={float(row['theta_E_analytic']):.8g}  "
            f"rel_err={float(row['relative_error']):.4g}"
        )
    print(f"max |rel_err|: {max_abs_err:.4g}")
    print(f"within 50% of analytic: {toward_prediction}")
    print(f"total runtime: {total_elapsed:.1f}s")
    print(f"results: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
