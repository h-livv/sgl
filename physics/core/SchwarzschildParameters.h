#pragma once

namespace Spacetime {

// Schwarzschild radius rs in geometrized units (G = c = 1).
// Vacuum exterior of a non-rotating spherical mass — not Kerr, not a plasma.
// Default rs = 1 is the canonical laboratory scale used by imaging and tests.
// Usage: PropagationContext constructs SchwarzschildMetric(parameters.rs) from this;
// Lens.parameters carries the same rs for horizon/geometry checks.
struct SchwarzschildParameters {
    double rs = 1.0;
};

} // namespace Spacetime
