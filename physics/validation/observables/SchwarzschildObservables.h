#pragma once

#include <cmath>

#include <core/SchwarzschildParameters.h>
#include <core/GeodesicState.h>

namespace Physics::Observables {

inline double schwarzschild_f(double r, double rs) {
    return 1.0 - rs / r;
}

inline double conserved_energy(const State& state, double rs) {
    const double r = state.X[1];
    return schwarzschild_f(r, rs) * state.U[0];
}

inline double conserved_angular_momentum(const State& state) {
    const double r = state.X[1];
    const double theta = state.X[2];
    const double sin_theta = std::sin(theta);
    return r * r * sin_theta * sin_theta * state.U[3];
}

inline double null_hamiltonian(const State& state, double rs) {
    const double r = state.X[1];
    const double theta = state.X[2];
    const double f = schwarzschild_f(r, rs);
    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vth = state.U[2];
    const double vph = state.U[3];
    const double sin_theta = std::sin(theta);
    return -f * vt * vt + (1.0 / f) * vr * vr + r * r * vth * vth +
           r * r * sin_theta * sin_theta * vph * vph;
}

inline double null_hamiltonian_error(const State& state, double rs) {
    const double r = state.X[1];
    const double H = null_hamiltonian(state, rs);
    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vph = state.U[3];
    const double scale = std::abs(vt * vt) + std::abs(vr * vr) + std::abs(r * r * vph * vph) + 1e-12;
    return std::abs(H) / scale;
}

inline double critical_impact_parameter(double rs) {
    return (3.0 * std::sqrt(3.0) / 2.0) * rs;
}

} // namespace Physics::Observables
