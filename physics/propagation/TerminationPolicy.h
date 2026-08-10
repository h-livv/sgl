#pragma once

#include <core/GeodesicState.h>

namespace Propagation {

class TerminationPolicy {
public:
    virtual ~TerminationPolicy() = default;
    virtual bool should_terminate(const State& state) const = 0;
};

class RadiusBoundTermination : public TerminationPolicy {
public:
    RadiusBoundTermination(double r_min, double r_max);

    bool should_terminate(const State& state) const override;

private:
    double r_min_;
    double r_max_;
};

} // namespace Propagation
