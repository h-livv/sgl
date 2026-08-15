#pragma once

// SI conversion helper (G, c, solar mass/radius, rs = 2GM/c^2 in metres).
// Not on the imaging execution path: no implementation .cpp includes this header.
// Runtime integration uses geometrized rs (SchwarzschildParameters::rs, default 1),
// not the SI value from schwarzschild_radius().

namespace Constants {

constexpr double G = 6.67430e-11;
constexpr double c = 299792458.0;

constexpr double solar_mass = 1.98847e30;
constexpr double solar_radius_m = 6.957e8;

// SI Schwarzschild radius 2GM/c^2 [m] for mass [kg]. Off-path helper only.
inline double schwarzschild_radius(double mass) {
    return 2.0 * G * mass / (c * c);
}

} // namespace Constants
