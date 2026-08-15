#pragma once

// Physics uses geometrized units (G = c = 1) unless otherwise noted.
// Canonical Schwarzschild radius is rs = 1 (see SchwarzschildParameters).
// Affine parameter λ and IntegrationSettings.step_size share this length/time unit.
// PhysicalConstants.h provides SI anchors for solar-scale conversions.
// Neither this header nor PhysicalConstants.h is included by the imaging execution path;
// this file is a units contract comment, not a conversion library.
