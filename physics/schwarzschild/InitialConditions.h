#pragma once

namespace Schwarzschild {

// Which constraint to use when filling U^t from spatial velocity (vt == 0).
enum class GeodesicKind {
    Timelike,
    Null,
};

// Equatorial timelike validation orbit (not the imaging path). Defaults are
// geometrized (rs typically 1); θ = π/2.
struct BoundOrbitInitialConditions {
    double t0 = 0.0;
    double r0 = 6.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
    double vr = 0.0;
    double vtheta = 0.0;
    double vphi = 0.06;
};

// Radial timelike validation: kinematics of rest-at-infinity rain, launched
// from r0 (not dropped from rest at r0). Not the imaging path.
struct RadialFreefallInitialConditions {
    double t0 = 0.0;
    double r0 = 10.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
};

// 1D point-source photon. `impact_parameter` is conserved L/E, not a Cartesian
// miss distance. <= 0 selects b_crit + impact_parameter_offset.
struct NullScatterInitialConditions {
    double t0 = 0.0;
    double r0 = 10.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
    double impact_parameter = 0.0;
    double impact_parameter_offset = 1e-3;
};

// Explicit (X, U_spatial) for parallel / 2D launches. vt == 0 is the sentinel
// that fills U^t from the null or timelike constraint.
struct CustomInitialConditions {
    double t0 = 0.0;
    double r0 = 6.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
    double vt = 0.0;
    double vr = 0.0;
    double vtheta = 0.0;
    double vphi = 0.0;
};

} // namespace Schwarzschild
