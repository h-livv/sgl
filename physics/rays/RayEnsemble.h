#pragma once

#include "Ray.h"

#include <cstddef>
#include <vector>

namespace Rays {

// Owns its rays by value. Sole authority for ray ids.
// Invariant: for all i, rays()[i].id == i.
class RayEnsemble {
public:
    RayEnsemble() = default;
    explicit RayEnsemble(const std::vector<State>& initial_states);

    // Appends a ray with id == the pre-insertion size. Returns the assigned id.
    std::size_t add(const State& initial_state);

    // Throws std::out_of_range if index >= size().
    const Ray& at(std::size_t index) const;
    void set_initial_state(std::size_t index, const State& initial_state);

    std::size_t size() const { return rays_.size(); }
    bool empty() const { return rays_.empty(); }
    const std::vector<Ray>& rays() const { return rays_; }

    std::vector<Ray>::const_iterator begin() const { return rays_.begin(); }
    std::vector<Ray>::const_iterator end() const { return rays_.end(); }

private:
    std::vector<Ray> rays_;
};

} // namespace Rays
