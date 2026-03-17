#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace custom_phase1 {

struct Result {
    bool feasible = false;
    long double max_violation = std::numeric_limits<long double>::infinity();
    long double t = std::numeric_limits<long double>::infinity();
    size_t iterations = 0;
    std::vector<long double> x;
};

static inline bool gaussian_solve_inplace(std::vector<long double>& A, std::vector<long double>& b, size_t n) {
    const long double eps = 1e-24L;
    for (size_t col = 0; col < n; ++col) {
        size_t piv = col;
        long double best = fabsl(A[col * n + col]);
        for (size_t r = col + 1; r < n; ++r) {
            long double v = fabsl(A[r * n + col]);
            if (v > best) {
                best = v;
                piv = r;
            }
        }
        if (best < eps) return false;

        if (piv != col) {
            for (size_t c = col; c < n; ++c) std::swap(A[col * n + c], A[piv * n + c]);
            std::swap(b[col], b[piv]);
        }

        long double diag = A[col * n + col];
        for (size_t r = col + 1; r < n; ++r) {
            long double f = A[r * n + col] / diag;
            if (f == 0.0L) continue;
            A[r * n + col] = 0.0L;
            for (size_t c = col + 1; c < n; ++c) {
                A[r * n + c] -= f * A[col * n + c];
            }
            b[r] -= f * b[col];
        }
    }

    for (size_t i = n; i-- > 0;) {
        long double s = b[i];
        for (size_t c = i + 1; c < n; ++c) s -= A[i * n + c] * b[c];
        b[i] = s / A[i * n + i];
    }

    return true;
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

    std::vector<long double> x(d, 0.0L);

    auto max_violation = [&](const std::vector<long double>& xx) -> long double {
        long double mv = -std::numeric_limits<long double>::infinity();
        for (size_t i = 0; i < m; ++i) {
            long double ax = 0.0L;
            for (size_t j = 0; j < d; ++j) ax += A[i][j] * xx[j];
            mv = std::max(mv, ax - rhs[i]);
        }
        return mv;
    };

    long double t0 = 0.0L;
    for (size_t i = 0; i < m; ++i) t0 = std::max(t0, -rhs[i]);
    long double t = t0 + 1.0L;

    const long double mu_factor = 0.2L;
    long double mu = 1.0L;
    const long double mu_min = 1e-18L;
    const size_t max_outer = 140;
    const size_t max_newton = 240;
    const long double grad_tol_floor = 1e-22L;
    const long double x_quad_eps = 1e-16L;

    auto objective = [&](const std::vector<long double>& xx, long double tt, long double muu) -> long double {
        if (!(tt > 0.0L) || !std::isfinite((double)tt)) return std::numeric_limits<long double>::infinity();
        long double val = tt;
        long double x2 = 0.0L;
        for (long double v : xx) x2 += v * v;
        val += 0.5L * x_quad_eps * x2;
        long double sum_log = logl(tt);
        for (size_t i = 0; i < m; ++i) {
            long double ax = 0.0L;
            for (size_t j = 0; j < d; ++j) ax += A[i][j] * xx[j];
            long double s = rhs[i] + tt - ax;
            if (!(s > 0.0L) || !std::isfinite((double)s)) return std::numeric_limits<long double>::infinity();
            sum_log += logl(s);
        }
        return val - muu * sum_log;
    };

    size_t total_iterations = 0;

    for (size_t outer = 0; outer < max_outer && mu >= mu_min; ++outer) {
        const long double grad_tol = std::max(grad_tol_floor, mu * 1e-4L);

        for (size_t it = 0; it < max_newton; ++it) {
            total_iterations++;

            std::vector<long double> w(m, 0.0L);
            std::vector<long double> di(m, 0.0L);
            long double sum_w = 0.0L;
            long double sum_di = 0.0L;

            bool interior = true;
            for (size_t i = 0; i < m; ++i) {
                long double ax = 0.0L;
                for (size_t j = 0; j < d; ++j) ax += A[i][j] * x[j];
                long double s = rhs[i] + t - ax;
                if (!(s > 0.0L)) {
                    interior = false;
                    break;
                }
                w[i] = 1.0L / s;
                di[i] = mu * w[i] * w[i];
                sum_w += w[i];
                sum_di += di[i];
            }
            if (!interior) break;

            std::vector<long double> grad_x(d, 0.0L);
            for (size_t i = 0; i < m; ++i) {
                long double coeff = mu * w[i];
                for (size_t j = 0; j < d; ++j) grad_x[j] += A[i][j] * coeff;
            }
            for (size_t j = 0; j < d; ++j) grad_x[j] += x_quad_eps * x[j];
            long double grad_t = 1.0L - mu * sum_w - mu / t;

            long double grad_inf = fabsl(grad_t);
            for (size_t j = 0; j < d; ++j) grad_inf = std::max(grad_inf, fabsl(grad_x[j]));
            if (grad_inf < grad_tol) break;

            const size_t n = d + 1;
            std::vector<long double> h(n * n, 0.0L);
            std::vector<long double> rhs_step(n, 0.0L);

            for (size_t i = 0; i < m; ++i) {
                long double s = di[i];
                if (s == 0.0L) continue;
                for (size_t p = 0; p < d; ++p) {
                    long double ap = A[i][p];
                    if (ap == 0.0L) continue;
                    for (size_t q = 0; q < d; ++q) {
                        h[p * n + q] += s * ap * A[i][q];
                    }
                }
            }
            for (size_t p = 0; p < d; ++p) h[p * n + p] += x_quad_eps;

            std::vector<long double> hxt(d, 0.0L);
            for (size_t i = 0; i < m; ++i) {
                long double s = di[i];
                for (size_t p = 0; p < d; ++p) hxt[p] += s * A[i][p];
            }
            for (size_t p = 0; p < d; ++p) {
                h[p * n + d] = -hxt[p];
                h[d * n + p] = -hxt[p];
            }
            h[d * n + d] = sum_di + mu / (t * t);

            for (size_t p = 0; p < d; ++p) rhs_step[p] = -grad_x[p];
            rhs_step[d] = -grad_t;

            if (!gaussian_solve_inplace(h, rhs_step, n)) break;

            long double alpha = 1.0L;
            if (rhs_step[d] < 0.0L) alpha = std::min(alpha, 0.99L * t / (-rhs_step[d]));
            for (size_t i = 0; i < m; ++i) {
                long double adx = 0.0L;
                for (size_t j = 0; j < d; ++j) adx += A[i][j] * rhs_step[j];
                long double ds = rhs_step[d] - adx;
                if (ds < 0.0L) {
                    long double s = 1.0L / w[i];
                    alpha = std::min(alpha, 0.99L * s / (-ds));
                }
            }
            alpha = std::clamp(alpha, 0.0L, 1.0L);

            long double f0 = objective(x, t, mu);
            bool accepted = false;
            for (int ls = 0; ls < 90; ++ls) {
                std::vector<long double> x2 = x;
                for (size_t j = 0; j < d; ++j) x2[j] += alpha * rhs_step[j];
                long double t2 = t + alpha * rhs_step[d];
                long double f2 = objective(x2, t2, mu);
                if (std::isfinite((double)f2) && f2 < f0) {
                    x.swap(x2);
                    t = t2;
                    accepted = true;
                    break;
                }
                alpha *= 0.5L;
            }
            if (!accepted) break;
        }

        long double mv = max_violation(x);
        if (mv <= tol && mu < 1e-12L) break;
        mu *= mu_factor;
    }

    out.iterations = total_iterations;
    out.max_violation = max_violation(x);
    out.t = t;
    out.x = std::move(x);
    out.feasible = (out.max_violation <= tol);
    return out;
}

}  // namespace custom_phase1
