#include "support/Check.h"

#include <rays/RayEnsemble.h>

#include <stdexcept>
#include <vector>

// Rays: RayEnsemble id/index invariant and copy isolation.
// Contract: add() and the vector constructor assign id == insertion index;
//           set_initial_state mutates one ray without renaming ids or aliasing
//           neighbors/copies.
// Pipeline: rays (sgl_rays). EnsemblePropagator and arrivals index by this id.
// Caveat: ids are never reused or reordered.

namespace {

bool equal_state_bits(const State& a, const State& b) {
    return a.X[0] == b.X[0] && a.X[1] == b.X[1] && a.X[2] == b.X[2] && a.X[3] == b.X[3] &&
           a.U[0] == b.U[0] && a.U[1] == b.U[1] && a.U[2] == b.U[2] && a.U[3] == b.U[3];
}

State make_state(double marker) {
    return State(Eigen::Vector4d(0.0, 10.0 + marker, 1.5, marker),
                 Eigen::Vector4d(1.0, -1.0, 0.0, marker));
}

} // namespace

int main() {
    Rays::RayEnsemble empty_ensemble;
    CHECK(empty_ensemble.empty(), "default ensemble is empty");
    CHECK(empty_ensemble.size() == 0, "default ensemble size is zero");

    const State s0 = make_state(0.0);
    const State s1 = make_state(1.0);
    const State s2 = make_state(2.0);

    Rays::RayEnsemble ensemble;
    CHECK(ensemble.add(s0) == 0, "first add returns id 0");
    CHECK(ensemble.add(s1) == 1, "second add returns id 1");
    CHECK(ensemble.add(s2) == 2, "third add returns id 2");
    CHECK(ensemble.size() == 3, "ensemble size after three adds");
    CHECK(!ensemble.empty(), "populated ensemble is not empty");

    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).id == i, "ray id equals index");
    }
    CHECK(equal_state_bits(ensemble.at(0).initial_state, s0), "ray 0 state preserved");
    CHECK(equal_state_bits(ensemble.at(1).initial_state, s1), "ray 1 state preserved");
    CHECK(equal_state_bits(ensemble.at(2).initial_state, s2), "ray 2 state preserved");

    const std::vector<State> states{s0, s1, s2};
    const Rays::RayEnsemble from_vector(states);
    CHECK(from_vector.size() == 3, "vector constructor size");
    for (std::size_t i = 0; i < from_vector.size(); ++i) {
        CHECK(from_vector.at(i).id == i, "vector constructor assigns sequential ids");
        CHECK(equal_state_bits(from_vector.at(i).initial_state, states[i]),
              "vector constructor preserves order");
    }

    try {
        (void)ensemble.at(ensemble.size());
        CHECK(false, "expected std::out_of_range from at()");
    } catch (const std::out_of_range&) {
    }

    try {
        ensemble.set_initial_state(ensemble.size(), s0);
        CHECK(false, "expected std::out_of_range from set_initial_state()");
    } catch (const std::out_of_range&) {
    }

    const State replacement = make_state(7.0);
    ensemble.set_initial_state(1, replacement);
    CHECK(equal_state_bits(ensemble.at(1).initial_state, replacement), "ray 1 was replaced");
    CHECK(equal_state_bits(ensemble.at(0).initial_state, s0), "ray 0 unaffected by ray 1 change");
    CHECK(equal_state_bits(ensemble.at(2).initial_state, s2), "ray 2 unaffected by ray 1 change");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).id == i, "ids unchanged after mutation");
    }

    Rays::RayEnsemble copy = ensemble;
    copy.set_initial_state(0, replacement);
    CHECK(equal_state_bits(ensemble.at(0).initial_state, s0), "copy mutation does not alias original");
    CHECK(equal_state_bits(copy.at(0).initial_state, replacement), "copy mutation applied to copy");

    Rays::RayEnsemble single;
    single.add(s0);
    CHECK(single.size() == 1, "single-ray ensemble size");
    CHECK(single.at(0).id == 0, "single-ray ensemble id");

    std::size_t iterated = 0;
    for (const Rays::Ray& ray : ensemble) {
        CHECK(ray.id == iterated, "iteration order matches ids");
        ++iterated;
    }
    CHECK(iterated == ensemble.size(), "iteration visits every ray");

    return TestSupport::report();
}
