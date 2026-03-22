#pragma once

#include <algorithm>
#include <limits>
#include <vector>

#if __has_include("interfaces/highs_c_api.h")
#include "interfaces/highs_c_api.h"
#elif __has_include("/mnt/c/Data/git/arnold-research/code/HiGHS/highs/interfaces/highs_c_api.h")
#include "/mnt/c/Data/git/arnold-research/code/HiGHS/highs/interfaces/highs_c_api.h"
#else
#error "highs_c_api.h not found. Add HiGHS include path or install headers."
#endif

namespace highs_phase1 {

struct Result {
    bool feasible = false;
    long double max_violation = std::numeric_limits<long double>::infinity();
    long double t = std::numeric_limits<long double>::infinity();
    std::vector<long double> x;
};

// Persistent HiGHS instance — created once, reused across solves.
// Avoids repeated creation/destruction of the global thread-pool
// scheduler that Highs_lpCall does (resetGlobalScheduler each call),
// which causes heap corruption ("double free") on long runs.
static void* g_highs = nullptr;

static inline void ensure_highs() {
    if (!g_highs) {
        g_highs = Highs_create();
        Highs_setBoolOptionValue(g_highs, "output_flag", 0);
    }
}

static inline Result solve(const std::vector<std::vector<long double>>& A, const std::vector<long double>& rhs, long double tol) {
    Result out;

    const size_t m = A.size();
    const size_t d = m ? A.front().size() : 0;

    if (m == 0) {
        out.feasible = true;
        out.max_violation = 0.0L;
        out.t = 0.0L;
        out.x.assign(d, 0.0L);
        return out;
    }

    ensure_highs();

    const double inf = 1.0e30;
    const size_t t_col = d;
    const size_t num_vars = d + 1;
    const HighsInt num_col = (HighsInt)num_vars;
    const HighsInt num_row = (HighsInt)m;

    std::vector<double> col_cost(num_vars, 0.0);
    std::vector<double> col_lower(num_vars, -inf);
    std::vector<double> col_upper(num_vars, +inf);
    col_cost[t_col] = 1.0;
    col_lower[t_col] = 0.0;
    std::vector<double> row_lower(m, -inf);
    std::vector<double> row_upper(m, 0.0);

    std::vector<HighsInt> a_start(m + 1, 0);
    std::vector<HighsInt> a_index;
    std::vector<double> a_value;
    a_index.reserve(m * ((d > 0 ? d : 1) + 1));
    a_value.reserve(m * ((d > 0 ? d : 1) + 1));

    for (size_t i = 0; i < m; ++i) {
        a_start[i] = (HighsInt)a_index.size();
        row_upper[i] = (double)rhs[i];
        for (size_t j = 0; j < d; ++j) {
            long double v = A[i][j];
            if (v == 0.0L) continue;
            a_index.push_back((HighsInt)j);
            a_value.push_back((double)v);
        }
        a_index.push_back((HighsInt)t_col);
        a_value.push_back(-1.0);
    }
    a_start[m] = (HighsInt)a_index.size();

    // Pass LP to persistent instance (replaces any previous model)
    HighsInt pass_status = Highs_passLp(
        g_highs,
        num_col,
        num_row,
        (HighsInt)a_index.size(),
        kHighsMatrixFormatRowwise,
        kHighsObjSenseMinimize,
        0.0,
        col_cost.data(),
        col_lower.data(),
        col_upper.data(),
        row_lower.data(),
        row_upper.data(),
        a_start.data(),
        a_index.data(),
        a_value.data()
    );

    if (pass_status == kHighsStatusError) {
        out.x.assign(d, 0.0L);
        return out;
    }

    HighsInt run_status = Highs_run(g_highs);
    HighsInt model_status = Highs_getModelStatus(g_highs);

    std::vector<double> col_value(num_vars, 0.0);
    std::vector<double> row_value(m, 0.0);
    Highs_getSolution(g_highs, col_value.data(), nullptr, row_value.data(), nullptr);

    // Clear model data but keep the instance and scheduler alive
    Highs_clearModel(g_highs);

    out.x.assign(d, 0.0L);
    for (size_t j = 0; j < d; ++j) out.x[j] = (long double)col_value[j];
    out.t = (long double)col_value[t_col];

    long double max_violation = -std::numeric_limits<long double>::infinity();
    for (size_t i = 0; i < m; ++i) {
        long double ax = 0.0L;
        for (size_t j = 0; j < d; ++j) ax += A[i][j] * out.x[j];
        max_violation = std::max(max_violation, ax - rhs[i]);
    }
    out.max_violation = max_violation;

    out.feasible = (run_status == kHighsStatusOk &&
                    model_status == kHighsModelStatusOptimal &&
                    out.t <= tol &&
                    out.max_violation <= tol);
    return out;
}

}  // namespace highs_phase1
