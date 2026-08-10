#include "schwarzschild/NullConstraint.h"
#include "support/Check.h"
#include "validation/observables/SchwarzschildObservables.h"

#include <Eigen/Dense>
#include <cmath>

int main() {
    constexpr double rs = 1.0;
    State state(Eigen::Vector4d(0.0, 10.0, 1.5707963267948966, 0.0),
                Eigen::Vector4d(10.0, -0.3, 0.0, 0.02));

    const Eigen::Vector4d x_before = state.X;
    const Eigen::Vector4d spatial_before(state.U[1], state.U[2], state.U[3], 0.0);

    const double H_before = Physics::Observables::null_hamiltonian(state, rs);
    Schwarzschild::project_onto_null_cone(state, rs);
    const double H_after = Physics::Observables::null_hamiltonian(state, rs);
    CHECK(std::abs(H_after) < std::abs(H_before), "projection did not improve null constraint");
    CHECK(std::abs(H_after) < 1e-14, "projection null constraint too high");

    CHECK_CLOSE(state.X[0], x_before[0], 0.0, "X0 changed");
    CHECK_CLOSE(state.X[1], x_before[1], 0.0, "X1 changed");
    CHECK_CLOSE(state.X[2], x_before[2], 0.0, "X2 changed");
    CHECK_CLOSE(state.X[3], x_before[3], 0.0, "X3 changed");
    CHECK_CLOSE(state.U[1], spatial_before[0], 0.0, "U1 changed");
    CHECK_CLOSE(state.U[2], spatial_before[1], 0.0, "U2 changed");
    CHECK_CLOSE(state.U[3], spatial_before[2], 0.0, "U3 changed");

    const double u0_once = state.U[0];
    Schwarzschild::project_onto_null_cone(state, rs);
    CHECK(std::abs(state.U[0] - u0_once) <= 1e-15 * std::max(1.0, std::abs(u0_once)),
          "projection not idempotent");

    return TestSupport::report();
}
