#pragma once
#include <Eigen/Dense>

// 8-component geodesic state in a chosen coordinate chart.
// X is the spacetime event; U = dX/dλ is the affine-parameter tangent (four-velocity).
// In the Schwarzschild spherical chart, X = (t, r, θ, φ). Photons are null: g_μν U^μ U^ν = 0.
// This is the value type for DynamicsModel, RK4, and propagation — not SGL lens geometry.
// operator+ / operator* exist so RK4 can combine stages; they have no geometric meaning.
struct State {
    Eigen::Vector4d X;
    Eigen::Vector4d U;

    State() {
        X = Eigen::Vector4d::Zero();
        U = Eigen::Vector4d::Zero();
    }

    State(Eigen::Vector4d pos, Eigen::Vector4d vel) {
        X = pos;
        U = vel;
    }

    State operator+(const State& other) const { return State(X + other.X, U + other.U); }
    State operator*(double scalar) const { return State(X * scalar, U * scalar); }
    friend State operator*(double scalar, const State& s) { return s * scalar; }
};
