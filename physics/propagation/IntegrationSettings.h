#pragma once

namespace Propagation {

struct IntegrationSettings {
    double step_size = 0.01;
    int max_steps = 100000;
};

} // namespace Propagation
