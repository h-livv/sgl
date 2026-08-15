#include "RayEnsemble.h"

#include <stdexcept>

namespace Rays {

RayEnsemble::RayEnsemble(const std::vector<State>& initial_states) {
    rays_.reserve(initial_states.size());
    for (const State& initial_state : initial_states) {
        add(initial_state);
    }
}

std::size_t RayEnsemble::add(const State& initial_state) {
    const std::size_t id = rays_.size();
    rays_.push_back(Ray{initial_state, id});
    return id;
}

const Ray& RayEnsemble::at(std::size_t index) const {
    if (index >= rays_.size()) {
        throw std::out_of_range("RayEnsemble: ray index out of range");
    }
    return rays_[index];
}

void RayEnsemble::set_initial_state(std::size_t index, const State& initial_state) {
    if (index >= rays_.size()) {
        throw std::out_of_range("RayEnsemble: ray index out of range");
    }
    // Keep id == index; only the launch State changes (Newton refinement).
    rays_[index].initial_state = initial_state;
}

} // namespace Rays
