#pragma once
#include <Eigen/Dense>

// Geodesic state: position X and tangent U in a chosen coordinate chart.
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
