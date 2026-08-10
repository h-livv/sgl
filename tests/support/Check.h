#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace TestSupport {

inline int g_failures = 0;

inline void check(bool condition, const char* file, int line, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, message);
    ++g_failures;
}

inline bool close_rel(double actual, double expected, double rel_tol) {
    const double scale = std::max({1.0, std::abs(expected), std::abs(actual)});
    return std::abs(actual - expected) <= rel_tol * scale;
}

inline int report() {
    if (g_failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    std::fprintf(stderr, "Total failures: %d\n", g_failures);
    return 1;
}

} // namespace TestSupport

#define CHECK(COND, MSG) TestSupport::check((COND), __FILE__, __LINE__, (MSG))
#define CHECK_CLOSE(ACTUAL, EXPECTED, REL_TOL, MSG) \
    TestSupport::check(TestSupport::close_rel((ACTUAL), (EXPECTED), (REL_TOL)), __FILE__, __LINE__, (MSG))
