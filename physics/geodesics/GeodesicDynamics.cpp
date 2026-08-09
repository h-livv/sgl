#include "GeodesicDynamics.h"

namespace Dynamics {

GeodesicDynamics::GeodesicDynamics(const Spacetime::Metric& metric) : metric_(metric) {}

State GeodesicDynamics::compute_derivative(const State& state) const {
    Eigen::Vector4d a = Eigen::Vector4d::Zero();

    for (int mu = 0; mu < 4; ++mu) {
        for (int alpha = 0; alpha < 4; ++alpha) {
            for (int beta = 0; beta < 4; ++beta) {
                double Gamma = metric_.christoffel(mu, alpha, beta, state.X);
                if (Gamma != 0.0) {
                    a[mu] -= Gamma * state.U[alpha] * state.U[beta];
                }
            }
        }
    }

    return State(state.U, a);
}

} // namespace Dynamics
