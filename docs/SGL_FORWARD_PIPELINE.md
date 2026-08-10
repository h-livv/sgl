# SGL Forward Pipeline

This guide documents the canonical end-to-end Schwarzschild gravitational-lensing
forward pipeline that produces the first Einstein-ring intensity image from null-geodesic
propagation.

## Prerequisites

- C++20 compiler
- CMake >= 3.22
- Eigen3

## Build

```bash
cmake -B build -S .
cmake --build build
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Canonical experiment

Run the full pipeline with default parameters:

```bash
./build/sgl_canonical_sgl_image --output-dir outputs/sgl_forward
```

### Configurable parameters

| Parameter | CLI flag | Default |
|-----------|----------|---------|
| Output directory | `--output-dir` | `outputs/sgl_forward` |
| Impact-parameter ray count | `--ray-count` | `41` |
| Azimuthal expansion count | `--azimuth-count` | `720` |
| Image resolution (square) | `--resolution` | `512` |
| Physical image extent | `--extent` | `40.0` |
| Minimum impact parameter | `--b-min` | `10.2` |
| Maximum impact parameter | `--b-max` | `11.6` |
| Integration step size | `--step-size` | `0.01` |
| Maximum integration steps | `--max-steps` | `300000` |

Fixed canonical physics (not exposed on the CLI):

- Schwarzschild radius `rs = 1.0`
- Source distance `30.0`
- Observer distance `30.0`
- Image plane half-width/half-height = `extent / 2`
- Horizon safety factor `1.0001`
- Escape radius = infinity
- Null-constraint projection enabled every `1000` steps

Example with explicit parameters:

```bash
./build/sgl_canonical_sgl_image \
  --output-dir outputs/sgl_forward \
  --ray-count 41 \
  --azimuth-count 720 \
  --resolution 512 \
  --extent 40.0 \
  --b-min 10.2 \
  --b-max 11.6 \
  --step-size 0.01 \
  --max-steps 300000
```

## Output files

The executable writes three files under the output directory:

- `einstein_ring.csv` — machine-readable normalized scalar intensity grid with metadata comments
- `einstein_ring.pgm` — ASCII PGM (`P2`) visual representation of the normalized image
- `run_summary.txt` — CLI settings, arrival counts, raw image maximum, and output paths

## Viewing the image

Open the PGM with any PGM-capable viewer. On Linux:

```bash
xdg-open outputs/sgl_forward/einstein_ring.pgm
```

The PGM rows are written from high `v` to low `v`, so positive `v` appears visually upward in common image viewers.

## Interpretation

The output is a normalized ray-count image:

1. Null geodesics are propagated from a 1D impact-parameter family.
2. Observer-plane arrivals are localized on the image plane.
3. Azimuthal symmetry expansion produces a 2D arrival distribution.
4. Each in-bounds arrival contributes unit intensity to one pixel.

The result is a geometric-optics intensity map, not a physically calibrated flux image.

## Current limitations

- Geometric optics only (unit weights, no radiometry)
- No point-spread function or diffraction
- No plasma, corona, or solar limb models
- No physical flux calibration
- No convergence study or adaptive sampling
- No GPU acceleration or parallel execution

## Troubleshooting

- **Empty or all-zero image:** widen `--b-min`/`--b-max`, increase `--max-steps`, or check `run_summary.txt` for `arrived_count` and `plane_arrivals`.
- **Ring clipped at edges:** increase `--extent` or reduce `--resolution` only after confirming the ring radius fits within the physical bounds.
- **Build failures:** verify Eigen3 is installed and CMake can find it (`find_package(Eigen3 CONFIG REQUIRED)`).
- **Tests fail after parameter changes:** the canonical defaults are validated by `canonical_image_pipeline`; keep physics fixed when tuning only output parameters.
