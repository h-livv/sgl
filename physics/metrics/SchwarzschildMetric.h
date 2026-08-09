#pragma once
#include <core/Metric.h>

namespace Spacetime {

class SchwarzschildMetric : public Metric {
public:
    explicit SchwarzschildMetric(double rs);

    double christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const override;

    double get_rs() const { return rs_; }

private:
    double rs_;
};

} // namespace Spacetime
