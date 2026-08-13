# Source-distance experiment

This is the current source-distance physics. For the full call trace see
[`TECHNICAL_BRIEFING.md`](TECHNICAL_BRIEFING.md) §§6–7 and
[`SGL_FORWARD_PIPELINE.md`](SGL_FORWARD_PIPELINE.md).

## What is measured

The 1D executable finds the observer-hit geodesic (`residual_u(b) = 0`), maps
its incoming direction to gnomonic \(\rho\), and reports

\[
\theta_E = \arctan\rho, \qquad R_{\mathrm{equiv}} = D\,\rho.
\]

That scalar does **not** come from image pixels. The ring picture is a later
azimuthal copy of the same angle (`expand_angular_azimuthally`).

Driver: `scripts/source_distance_test.py` → `build/sgl_canonical_sgl_image`.
CTest mirror: `tests/source_distance_angular_behavior.cpp`.

## Independent / dependent variables

| Role | Quantity | Typical Experiment 1 value |
|---|---|---|
| Independent | source distance \(S\) | \(20, 50, 100, 200, 1000\), plus `"inf"` |
| Dependent | \(\theta_E\) (radians) | from `run_summary.txt` |
| Held fixed | \(r_s=1\), \(D=30\), on-axis, \(b\in[2,20]\), \(h=0.01\) | |

`"inf"` switches `--ray-model parallel` at a **finite** launch plane
(`source-distance=30` in that script). It is not \(S=\infty\).

## Expected behavior

\(\theta_E\) **increases** with \(S\) and approaches the parallel-beam stand-in.
That is the opposite of an older screen-intersection radius (fixed-\(b\) family
binned on an extended plane), which decreased with \(S\) and is **not** what
the current executables report.

Weak-field comparison is a **separate** script,
`scripts/weak_field_validation.py`, using

\[
R_E = \sqrt{2 r_s D S/(D+S)}, \qquad \theta_{\mathrm{an}} = \arctan(R_E/D)
\]

at \(D=200\). Experiment 1 (\(D=30\)) is not in the weak-field regime and does
not overlay that formula.
