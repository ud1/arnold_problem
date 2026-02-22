#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <limits>
#include <iomanip>
#include <array>

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    size_t n;
};

OMatrix makeOMatrix(std::vector<size_t> gens) {
    size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0); // fill 0 1 2 ...
    std::vector<std::vector<size_t>> intersections(n);

    for (size_t gen : gens)
    {
        size_t first = lines[gen];
        size_t second = lines[gen + 1];

        intersections[first].push_back(second);
        intersections[second].push_back(first);

        lines[gen] = second;
        lines[gen + 1] = first;
    }

    return OMatrix{intersections, n};
}

std::vector<size_t> gens_str_to_vec(const std::string& s) {
    std::vector<size_t> numbers;
    std::stringstream ss(s);
    size_t temp_int;

    while (ss >> temp_int) {
        numbers.push_back(temp_int);
    }

    return numbers;
}

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\"\n"
        << "  " << argv0 << " --gens-file path/to/gens.txt\n"
        << "  " << argv0 << " 0 1 0 2 ...\n"
        << "  " << argv0 << " [--max-steps N]\n"
        << "\n"
        << "Notes:\n"
        << "  Default is 'no limit' on iterations.\n"
        << "  --max-steps N sets a limit; --max-steps 0 means 'no limit'.\n"
        << "  Optimization/logging hyperparameters are fixed in code.\n"
        << "\n"
        << "Generators are 0-indexed adjacent swaps (s_i swaps i and i+1).\n";
}

static bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// "Margin" used in hinge penalties max(raw + min_*, 0)^2.
// Strict feasibility is always checked against raw < 0.
static double g_min_edge = 1.0;
static double g_min_angle = 0.01;
static double g_reg_m_l2 = 1e-4;
static double g_reg_b_l2 = 1e-6;
static double g_reg_x_spread = 1e-4;
static double g_reg_x_soft = 10.0;
static double g_reg_y_spread = 1e-4;
static double g_reg_y_soft = 10.0;
static double g_reg_edge_ratio = 2e-3;
static double g_reg_edge_eps = 1e-12;
static double g_reg_lse_temperature = 5.0;
static double g_reg_activation_loss = 1e-3;
static double g_reg_scale_smooth = 0.02;
static double g_reg_runtime_scale = 0.0;

struct EdgeTerm {
    size_t p1, p2, p3, p4, p5, p6, p7, p8;

    EdgeTerm(size_t p1, size_t p2, size_t p3, size_t p4, size_t p5, size_t p6, size_t p7, size_t p8) :
        p1(p1), p2(p2), p3(p3), p4(p4), p5(p5), p6(p6), p7(p7), p8(p8) {}

    double value2(const std::vector<double> &params) const
    {
        double result = value1(params);
        return result * result;
    }

    double value1(const std::vector<double> &params) const
    {
        double result = raw(params) + g_min_edge;

        if (result < 0.0)
            return 0.0;

        return result;
    }

    double raw(const std::vector<double> &params) const
    {
        // (bk - bi) * (mi - mj) - (bj - bi) * (mi - mk) < 0
        return (params[p1] - params[p2]) * (params[p3] - params[p4])
                        - (params[p5] - params[p6]) * (params[p7] - params[p8]);
    }

    bool satisfied(const std::vector<double> &params)
    {
        return raw(params) < 0.0;
    }

    void grad(const std::vector<double> &params, std::vector<double> &grad_out) const
    {
        double val = value1(params) * 2.0;
        if (val == 0.0)
            return;

        double m1 = (params[p1] - params[p2]);
        double m2 = (params[p3] - params[p4]);
        double m3 = (params[p5] - params[p6]);
        double m4 = (params[p7] - params[p8]);

        grad_out[p1] += val * m2;
        grad_out[p2] -= val * m2;
        grad_out[p3] += val * m1;
        grad_out[p4] -= val * m1;
        grad_out[p5] -= val * m4;
        grad_out[p6] += val * m4;
        grad_out[p7] -= val * m3;
        grad_out[p8] += val * m3;
    }
};

struct AngleTerm {
    size_t p1, p2;
    AngleTerm(size_t p1, size_t p2) : p1(p1), p2(p2) {}

    double value2(const std::vector<double> &params) const {
        double result = value1(params);
        return result * result;
    }

    double value1(const std::vector<double> &params) const {
        double result = raw(params) + g_min_angle;
        if (result < 0.0)
            return 0.0;

        return result;
    }

    double raw(const std::vector<double> &params) const {
        // m[i] - m[i + 1] < 0
        return params[p1] - params[p2];
    }

    bool satisfied(const std::vector<double> &params) {
        return raw(params) < 0.0;
    }

    void grad(const std::vector<double> &params, std::vector<double> &grad_out) const {
        double val = value1(params) * 2.0;
        if (val == 0.0)
            return;

        grad_out[p1] += val;
        grad_out[p2] -= val;
    }
};

struct Terms {
    std::vector<EdgeTerm> edgeTerms;
    std::vector<AngleTerm> angleTerms;
    std::vector<double> params;
    std::vector<double> saved_params;
    std::vector<double> grad;
    std::vector<double> prev_grad;
    size_t n;

    struct SatStats {
        bool strict_ok;
        bool margin_ok;
        size_t strict_violations;
        size_t margin_violations;
        double worst_raw;        // max raw (should be < 0 for strict)
        double worst_shifted;    // max(raw + MIN_*) (should be < 0 for margin_ok)
    };

    struct GeoStats {
        size_t pair_count;
        size_t segment_count;
        double min_edge;
        double max_edge;
        double global_edge_ratio;
        double max_abs_x_intersection;
    };

    struct IntersectionPoint {
        double x;
        double y;
        std::array<size_t, 4> idx;
        std::array<double, 4> dx;
        std::array<double, 4> dy;
    };

    IntersectionPoint make_intersection_point(size_t i, size_t j) const {
        double mi = params[i];
        double mj = params[j];
        double bi = params[i + n];
        double bj = params[j + n];
        double denom = mi - mj;
        if (std::abs(denom) < 1e-12) {
            denom = (denom >= 0.0) ? 1e-12 : -1e-12;
        }
        double num = bj - bi;
        double x = num / denom;
        double y = mi * x + bi;
        double dxdmi = -(num) / (denom * denom);
        double dxdmj =  (num) / (denom * denom);
        double dxdbi = -1.0 / denom;
        double dxdbj =  1.0 / denom;
        double dydmi = x + mi * dxdmi;
        double dydmj = mi * dxdmj;
        double dydbi = 1.0 + mi * dxdbi;
        double dydbj = mi * dxdbj;
        return IntersectionPoint{
            x,
            y,
            {i, j, i + n, j + n},
            {dxdmi, dxdmj, dxdbi, dxdbj},
            {dydmi, dydmj, dydbi, dydbj}
        };
    }

    std::vector<IntersectionPoint> cached_ix;

    size_t pair_idx(size_t a, size_t b) const {
        if (a > b) std::swap(a, b);
        size_t base = a * (2 * n - a - 1) / 2;
        return base + (b - a - 1);
    }

    void precompute_intersections() {
        size_t npairs = n * (n - 1) / 2;
        cached_ix.resize(npairs);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                cached_ix[pair_idx(i, j)] = make_intersection_point(i, j);
            }
        }
    }

    // LogSumExp approximation of log(edge_ratio): soft_log_max - soft_log_min
    // Returns the value; optionally writes gradient scaled by `scale` into grad_out.
    double edge_ratio_lse_grad(std::vector<double>& grad_out, double scale) const {
        struct Segment {
            double z;
            double inv_e;
            std::array<size_t, 8> idx;
            std::array<double, 8> de;
            size_t count;
        };

        std::vector<const IntersectionPoint*> pts;
        pts.reserve(n > 0 ? n - 1 : 0);

        std::vector<Segment> segs;
        segs.reserve(n > 1 ? n * (n - 2) : 0);

        auto add_deriv = [](Segment& s, size_t idx, double val) {
            for (size_t t = 0; t < s.count; ++t) {
                if (s.idx[t] == idx) {
                    s.de[t] += val;
                    return;
                }
            }
            s.idx[s.count] = idx;
            s.de[s.count] = val;
            s.count++;
        };

        for (size_t i = 0; i < n; ++i) {
            pts.clear();
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                pts.push_back(&cached_ix[pair_idx(i, j)]);
            }
            std::sort(pts.begin(), pts.end(), [](const IntersectionPoint* a, const IntersectionPoint* b) {
                if (a->x != b->x) return a->x < b->x;
                return a->y < b->y;
            });
            for (size_t k = 1; k < pts.size(); ++k) {
                const auto& a = *pts[k - 1];
                const auto& b = *pts[k];
                double dx = b.x - a.x;
                double dy = b.y - a.y;
                double e = std::hypot(dx, dy);
                double safe_e = std::max(e, g_reg_edge_eps);
                Segment seg{};
                seg.z = std::log(safe_e);
                seg.inv_e = 1.0 / safe_e;
                seg.count = 0;
                for (size_t t = 0; t < 4; ++t) {
                    double dedp = (dx * (-a.dx[t]) + dy * (-a.dy[t])) * seg.inv_e;
                    add_deriv(seg, a.idx[t], dedp);
                }
                for (size_t t = 0; t < 4; ++t) {
                    double dedp = (dx * b.dx[t] + dy * b.dy[t]) * seg.inv_e;
                    add_deriv(seg, b.idx[t], dedp);
                }
                segs.push_back(seg);
            }
        }

        if (segs.empty()) return 0.0;

        double T = g_reg_lse_temperature;
        double z_max = -std::numeric_limits<double>::infinity();
        double z_min =  std::numeric_limits<double>::infinity();
        for (const auto& seg : segs) {
            z_max = std::max(z_max, seg.z);
            z_min = std::min(z_min, seg.z);
        }

        double sum_exp_max = 0.0, sum_exp_min = 0.0;
        for (const auto& seg : segs) {
            sum_exp_max += std::exp(T * (seg.z - z_max));
            sum_exp_min += std::exp(-T * (seg.z - z_min));
        }

        double soft_log_max = z_max + std::log(sum_exp_max) / T;
        double soft_log_min = z_min - std::log(sum_exp_min) / T;

        if (scale != 0.0) {
            double inv_sum_max = 1.0 / sum_exp_max;
            double inv_sum_min = 1.0 / sum_exp_min;
            for (const auto& seg : segs) {
                double w_max = std::exp(T * (seg.z - z_max)) * inv_sum_max;
                double w_min = std::exp(-T * (seg.z - z_min)) * inv_sum_min;
                double coeff = scale * (w_max - w_min);
                for (size_t t = 0; t < seg.count; ++t) {
                    grad_out[seg.idx[t]] += coeff * seg.de[t] * seg.inv_e;
                }
            }
        }

        return soft_log_max - soft_log_min;
    }

    Terms(OMatrix &o) {
        n = o.n;

        for (size_t i = 0; i < o.n; ++i) {
            params.push_back((double) i - (double) o.n * 0.5);
            grad.push_back(0.0);
        }
        for (size_t i = 0; i < o.n; ++i) {
            params.push_back(0.0);
            grad.push_back(0.0);
        }

        saved_params.resize(params.size());
        prev_grad.resize(params.size());

        size_t line_count = n;
        auto m = [](size_t i){return i;};
        auto b = [line_count](size_t i){return i + line_count;};


        for (size_t i = 0; i < o.n-1; ++i) {
            angleTerms.emplace_back(m(i), m(i + 1));
        }

        for (size_t i = 0; i < o.n; ++i) {
            auto &line = o.intersections[i];
            if (line.size() < 2) {
                continue;
            }

            for (size_t ind = 0; ind < line.size() - 1; ++ind) {
                size_t j = line[ind];
                size_t k = line[ind + 1];

                // y = m*x + b
                // x_ij = (bj - bi) / (mi - mj)
                // x_ik = (bk - bi) / (mi - mk)
                // x_ik < x_ij

                if (i > j && i > k) {
                    // (bk - bi) * (mi - mj) < (bj - bi) * (mi - mk)
                    edgeTerms.emplace_back(
                            b(k), b(i),
                            m(i), m(j),
                            b(j), b(i),
                            m(i), m(k));
                }
                else if (i > j && i < k) {
                    // (bi - bk) * (mi - mj) < (bj - bi) * (mk - mi)
                    edgeTerms.emplace_back(
                            b(i), b(k),
                            m(i), m(j),
                            b(j), b(i),
                            m(k), m(i));
                }
                else if (i < j && i > k) {
                    // (bk - bi) * (mj - mi) < (bi - bj) * (mi - mk)
                    edgeTerms.emplace_back(
                            b(k), b(i),
                            m(j), m(i),
                            b(i), b(j),
                            m(i), m(k));
                }
                else { // (i < j && i < k)
                    // (bi - bk) * (mj - mi) < (bi - bj) * (mk - mi)
                    edgeTerms.emplace_back(
                            b(i), b(k),
                            m(j), m(i),
                            b(i), b(j),
                            m(k), m(i));
                }
            }
        }
    }

    double constraint_value() const {
        double result = 0.0;
        for (auto &t : angleTerms)
        {
            result += t.value2(params);
        }
        for (auto &t : edgeTerms)
        {
            result += t.value2(params);
        }
        return result;
    }

    double value() const
    {
        double result = constraint_value();
        double reg_scale = g_reg_runtime_scale;

        if (reg_scale > 0.0) {
            for (size_t i = 0; i < n; ++i) {
                double m = params[i];
                double b = params[i + n];
                result += reg_scale * g_reg_m_l2 * m * m;
                result += reg_scale * g_reg_b_l2 * b * b;
            }

            // Compute all intersection coordinates once
            size_t npairs = n * (n - 1) / 2;
            std::vector<double> ix(npairs), iy(npairs);
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    double denom = params[i] - params[j];
                    if (std::abs(denom) < 1e-12) {
                        denom = (denom >= 0.0) ? 1e-12 : -1e-12;
                    }
                    double x = (params[j + n] - params[i + n]) / denom;
                    double y = params[i] * x + params[i + n];
                    size_t idx = i * (2 * n - i - 1) / 2 + (j - i - 1);
                    ix[idx] = x;
                    iy[idx] = y;

                    double sx = x / std::max(g_reg_x_soft, 1e-30);
                    result += reg_scale * g_reg_x_spread * std::log1p(sx * sx);
                    double sy = y / std::max(g_reg_y_soft, 1e-30);
                    result += reg_scale * g_reg_y_spread * std::log1p(sy * sy);
                }
            }

            // LogSumExp approximation of log(edge_ratio)
            std::vector<double> zvals;
            zvals.reserve(n > 1 ? n * (n - 2) : 0);
            std::vector<std::pair<double, double>> pts;
            pts.reserve(n > 0 ? n - 1 : 0);

            auto pidx = [this](size_t a, size_t b) -> size_t {
                if (a > b) std::swap(a, b);
                return a * (2 * n - a - 1) / 2 + (b - a - 1);
            };

            for (size_t i = 0; i < n; ++i) {
                pts.clear();
                for (size_t j = 0; j < n; ++j) {
                    if (j == i) continue;
                    size_t idx = pidx(i, j);
                    pts.emplace_back(ix[idx], iy[idx]);
                }
                std::sort(pts.begin(), pts.end());
                for (size_t k = 1; k < pts.size(); ++k) {
                    double dx = pts[k].first - pts[k - 1].first;
                    double dy = pts[k].second - pts[k - 1].second;
                    double e = std::hypot(dx, dy);
                    double safe_e = std::max(e, g_reg_edge_eps);
                    zvals.push_back(std::log(safe_e));
                }
            }

            if (!zvals.empty()) {
                double T = g_reg_lse_temperature;
                double z_max = *std::max_element(zvals.begin(), zvals.end());
                double z_min = *std::min_element(zvals.begin(), zvals.end());
                double sum_exp_max = 0.0, sum_exp_min = 0.0;
                for (double z : zvals) {
                    sum_exp_max += std::exp(T * (z - z_max));
                    sum_exp_min += std::exp(-T * (z - z_min));
                }
                double soft_log_max = z_max + std::log(sum_exp_max) / T;
                double soft_log_min = z_min - std::log(sum_exp_min) / T;
                result += reg_scale * g_reg_edge_ratio * (soft_log_max - soft_log_min);
            }
        }
        return result;
    }

    double calc_grad()
    {
        for (auto &v : grad)
            v = 0.0;

        for (auto &t : angleTerms)
        {
            t.grad(params, grad);
        }

        for (auto &t : edgeTerms)
        {
            t.grad(params, grad);
        }

        double reg_scale = g_reg_runtime_scale;
        if (reg_scale > 0.0) {
            for (size_t i = 0; i < n; ++i) {
                grad[i] += reg_scale * 2.0 * g_reg_m_l2 * params[i];
                grad[i + n] += reg_scale * 2.0 * g_reg_b_l2 * params[i + n];
            }

            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    const auto& pt = cached_ix[pair_idx(i, j)];
                    double x = pt.x;
                    double x_soft2 = std::max(g_reg_x_soft * g_reg_x_soft, 1e-30);
                    double dphi_dx = reg_scale * g_reg_x_spread * (2.0 * x / (x_soft2 + x * x));
                    double coeff = dphi_dx;
                    for (size_t t = 0; t < 4; ++t) {
                        grad[pt.idx[t]] += coeff * pt.dx[t];
                    }

                    double y = pt.y;
                    double y_soft2 = std::max(g_reg_y_soft * g_reg_y_soft, 1e-30);
                    double dphi_dy = reg_scale * g_reg_y_spread * (2.0 * y / (y_soft2 + y * y));
                    for (size_t t = 0; t < 4; ++t) {
                        grad[pt.idx[t]] += dphi_dy * pt.dy[t];
                    }
                }
            }

            edge_ratio_lse_grad(grad, reg_scale * g_reg_edge_ratio);
        }

        double result = 0.0;
        for (auto &v : grad)
        {
            result += v*v;
        }

        return result;
    }

    void step(double v)
    {
        size_t size = params.size();
        for (size_t i = 0; i < size; ++i) {
            params[i] -= grad[i] * v;
        }
    }

    bool satisfied() {
        for (auto &t : angleTerms)
        {
            if (!t.satisfied(params))
            {
                return false;
            }
        }

        for (auto &t : edgeTerms)
        {
            if (!t.satisfied(params))
            {
                return false;
            }
        }

        return true;
    }

    SatStats sat_stats() const {
        SatStats st;
        st.strict_ok = true;
        st.margin_ok = true;
        st.strict_violations = 0;
        st.margin_violations = 0;
        st.worst_raw = -std::numeric_limits<double>::infinity();
        st.worst_shifted = -std::numeric_limits<double>::infinity();

        for (const auto& t : angleTerms) {
            double r = t.raw(params);
            st.worst_raw = std::max(st.worst_raw, r);
            st.worst_shifted = std::max(st.worst_shifted, r + g_min_angle);
            if (!(r < 0.0)) {
                st.strict_ok = false;
                st.strict_violations++;
            }
            if (!((r + g_min_angle) < 0.0)) {
                st.margin_ok = false;
                st.margin_violations++;
            }
        }

        for (const auto& t : edgeTerms) {
            double r = t.raw(params);
            st.worst_raw = std::max(st.worst_raw, r);
            st.worst_shifted = std::max(st.worst_shifted, r + g_min_edge);
            if (!(r < 0.0)) {
                st.strict_ok = false;
                st.strict_violations++;
            }
            if (!((r + g_min_edge) < 0.0)) {
                st.margin_ok = false;
                st.margin_violations++;
            }
        }

        return st;
    }

    GeoStats geo_stats() const {
        GeoStats gs{};
        gs.pair_count = 0;
        gs.segment_count = 0;
        gs.min_edge = std::numeric_limits<double>::infinity();
        gs.max_edge = 0.0;
        gs.global_edge_ratio = std::numeric_limits<double>::infinity();
        gs.max_abs_x_intersection = 0.0;

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                const auto& pt = cached_ix[pair_idx(i, j)];
                gs.pair_count++;
                gs.max_abs_x_intersection = std::max(gs.max_abs_x_intersection, std::abs(pt.x));
            }
        }

        std::vector<std::pair<double, double>> pts;
        pts.reserve(n > 0 ? n - 1 : 0);
        for (size_t i = 0; i < n; ++i) {
            pts.clear();
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                const auto& pt = cached_ix[pair_idx(i, j)];
                pts.emplace_back(pt.x, pt.y);
            }
            std::sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) {
                if (a.first != b.first) return a.first < b.first;
                return a.second < b.second;
            });
            for (size_t k = 1; k < pts.size(); ++k) {
                double e = std::hypot(pts[k].first - pts[k - 1].first, pts[k].second - pts[k - 1].second);
                gs.min_edge = std::min(gs.min_edge, e);
                gs.max_edge = std::max(gs.max_edge, e);
                gs.segment_count++;
            }
        }

        if (gs.segment_count > 0 && gs.min_edge > 0.0 && std::isfinite(gs.min_edge)) {
            gs.global_edge_ratio = gs.max_edge / gs.min_edge;
        }
        return gs;
    }

    void print() {
        size_t n = params.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            double m = params[i];
            double b = params[i + n];
            std::cout << "y = " << m << " * x ";
            if (b >= 0.0)
                std::cout << " +";
            std::cout << b << std::endl;
        }
    }

    void print_csv(std::ostream& out) const {
        size_t n = params.size() / 2;
        out << std::setprecision(17) << "m,b\n";
        for (size_t i = 0; i < n; ++i) {
            out << params[i] << "," << params[i + n] << "\n";
        }
        out << std::defaultfloat;
    }

    void print_tuples(std::ostream& out) const {
        size_t line_count = params.size() / 2;
        out << std::setprecision(17) << "[";
        for (size_t i = 0; i < line_count; ++i) {
            if (i != 0) out << ", ";
            out << "(" << params[i] << "," << params[i + line_count] << ")";
        }
        out << "]\n" << std::defaultfloat;
    }

    void print_json(std::ostream& out) const {
        size_t line_count = params.size() / 2;
        out << std::setprecision(17) << "[";
        for (size_t i = 0; i < line_count; ++i) {
            if (i != 0) out << ",";
            out << "{\"m\":" << params[i] << ",\"b\":" << params[i + line_count] << "}";
        }
        out << "]\n" << std::defaultfloat;
    }
};

int main(int argc, char** argv) {
    std::string gens_str =
        "1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 3 5 7 9 11 13 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 3 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 5 7 9 11 10 9 8 7 6 5 7 9 11 13 12 11 13 15 14 13 12 11 10 9 8 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 5 7 6 7 9 8 7 9 11 13 15 14 13 12 11 13 15 17 16 15 14 13 12 11 10 9 8 9 11 10 12 1 3 5 7 9 11 13 15 17";

    size_t max_steps_limit = 0;
    size_t LOG_EVERY = 100000;
    double step = 0.01;
    enum class LogFormat { Compact, Full };
    LogFormat log_format = LogFormat::Compact;
    bool unlimited_steps = true;
    for (int argi = 1; argi < argc; ++argi) {
        std::string arg = argv[argi];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "--gens") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --gens\n";
                print_usage(argv[0]);
                return 2;
            }
            gens_str = argv[++argi];
            continue;
        }
        if (arg == "--gens-file") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --gens-file\n";
                print_usage(argv[0]);
                return 2;
            }
            std::string file_contents;
            if (!read_file(argv[++argi], file_contents)) {
                std::cerr << "Failed to read file\n";
                return 2;
            }
            gens_str = file_contents;
            continue;
        }
        if (arg == "--max-steps") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --max-steps\n";
                return 2;
            }
            max_steps_limit = (size_t)std::stoull(argv[++argi]);
            if (max_steps_limit == 0) {
                unlimited_steps = true;
            } else {
                unlimited_steps = false;
            }
            continue;
        }
        if (!arg.empty() && arg[0] != '-') {
            // Positional numbers: ./prog 1 3 5 ...
            std::ostringstream ss;
            ss << arg;
            for (int j = argi + 1; j < argc; ++j) {
                ss << ' ' << argv[j];
            }
            gens_str = ss.str();
            break;
        }
        std::cerr << "Unknown option: " << arg << "\n";
        print_usage(argv[0]);
        return 2;
    }

    auto gens = gens_str_to_vec(gens_str);
    OMatrix o = makeOMatrix(gens);
    Terms terms{o};

    double best_result = std::numeric_limits<double>::infinity();
    size_t sat_hits = 0;
    double best_sat_edge_ratio = std::numeric_limits<double>::infinity();
    size_t iter_done = 0;
    const double STEP_DEC = 0.5;
    const double STEP_INC = 1.1;
    const double ARMIJO_C = 0.5;
    const double STEP_MIN = 1e-14;
    const double STEP_MAX = 1.0;
    g_reg_runtime_scale = 0.0;
    std::cout << "#LOG_BEGIN\n";
    terms.precompute_intersections();
    terms.calc_grad();
    for (size_t i = 0; unlimited_steps || i < max_steps_limit; ++i)
    {
        double constraint_val = terms.constraint_value();
        auto st_iter = terms.sat_stats();
        double target_reg_scale = 0.0;
        if (st_iter.strict_ok) {
            double x = constraint_val / std::max(g_reg_activation_loss, 1e-30);
            target_reg_scale = 1.0 / (1.0 + x * x);
        }
        double smooth = std::clamp(g_reg_scale_smooth, 0.0, 1.0);
        g_reg_runtime_scale += (target_reg_scale - g_reg_runtime_scale) * smooth;

        terms.prev_grad = terms.grad;
        iter_done = i;
        terms.precompute_intersections();
        double val = terms.value();
        if (val < best_result) best_result = val;
        double grad2 = terms.calc_grad(); // also updates terms.grad buffer
        double grad = std::sqrt(grad2);
        if (LOG_EVERY != 0 && i % LOG_EVERY == 0) {
            auto st = st_iter;
            auto gs = terms.geo_stats();
            if (log_format == LogFormat::Full) {
                std::cout
                    << i
                    << "] V " << val
                    << " | GRAD " << grad
                    << " | STEP " << step
                    << " | STRICT " << (st.strict_ok ? 1 : 0)
                    << " | MARGIN " << (st.margin_ok ? 1 : 0)
                    << " | worst_raw " << st.worst_raw
                    << " | worst_shifted " << st.worst_shifted
                    << " | viol(strict) " << st.strict_violations
                    << " | viol(margin) " << st.margin_violations
                    << " | edge_ratio " << gs.global_edge_ratio
                    << " | max_abs_x " << gs.max_abs_x_intersection
                    << " | cV " << constraint_val
                    << " | regS " << g_reg_runtime_scale
                    << std::endl;
            } else {
                // Compact: focus on what matters for convergence.
                // Fixed-width columns for easy diff/scan.
                // All floats use scientific notation to keep width stable across magnitudes.
                std::cout << std::setw(8) << i
                          << " V=" << std::setw(13) << std::scientific << std::setprecision(6) << val
                          << " G=" << std::setw(11) << std::scientific << std::setprecision(3) << grad
                          << " step=" << std::setw(11) << std::scientific << std::setprecision(3) << step
                          << " wr=" << std::setw(11) << std::scientific << std::setprecision(3) << st.worst_raw
                          << " er=" << std::setw(11) << std::scientific << std::setprecision(3) << gs.global_edge_ratio
                          << " cV=" << std::setw(11) << std::scientific << std::setprecision(3) << constraint_val
                          << " rS=" << std::setw(8)  << std::fixed << std::setprecision(3) << g_reg_runtime_scale
                          << " vS=" << std::setw(3) << std::dec << st.strict_violations
                          << " S=" << (st.strict_ok ? 1 : 0)
                          << std::defaultfloat
                          << std::endl;
            }

            if (st.strict_ok) {
                sat_hits++;
                if (gs.global_edge_ratio < best_sat_edge_ratio) {
                    best_sat_edge_ratio = gs.global_edge_ratio;
                    std::cout << "#SAT_HISTORY_BEGIN"
                              << " iter=" << i
                              << " V=" << val
                              << " edge_ratio=" << gs.global_edge_ratio
                              << " max_abs_x=" << gs.max_abs_x_intersection
                              << "\n";
                    terms.print_tuples(std::cout);
                    std::cout << "#SAT_HISTORY_END\n";
                }
            }
        }

        // Armijo backtracking; BB update on accepted step.
        bool accepted = false;
        for (int j = 0; j < 100; ++j) {
            double alpha = std::min(std::max(step, STEP_MIN), STEP_MAX);
            terms.saved_params = terms.params;
            terms.step(alpha);
            double new_val = terms.value();

            if (new_val - val > -ARMIJO_C * alpha * grad2) {
                step = alpha * STEP_DEC;
                terms.params = terms.saved_params;
                continue;
            }

            double dgrad_dot_dx = 0.0;
            for (size_t k = 0; k < terms.grad.size(); ++k) {
                dgrad_dot_dx += (terms.grad[k] - terms.prev_grad[k]) * terms.grad[k];
            }
            if (dgrad_dot_dx > 0.0) {
                step = std::min(std::max(alpha * (grad2 / dgrad_dot_dx), STEP_MIN), STEP_MAX);
            } else {
                step = std::min(alpha * STEP_INC, STEP_MAX);
            }
            accepted = true;
            break;
        }
        if (!accepted) {
            break; // can't find a decreasing step; report in SUMMARY
        }
    }
    std::cout << "#LOG_END\n";

    {
        // Final snapshot: everything needed for reading the result without scanning logs.
        // calc_grad() updates internal grad buffer; compute it before printing summary.
        terms.precompute_intersections();
        double final_val = terms.value();
        double final_grad = std::sqrt(terms.calc_grad());
        auto st = terms.sat_stats();
        auto gs = terms.geo_stats();
        double best_sat_edge_ratio_out = std::isfinite(best_sat_edge_ratio)
                                         ? best_sat_edge_ratio
                                         : -1.0;
        std::cout << "#RESULT_BEGIN\n";
        if (st.strict_ok) {
            std::cout
                << "SAT_FOUND"
                << " strict=1"
                << " margin=" << (st.margin_ok ? 1 : 0)
                << " worst_raw=" << st.worst_raw
                << " worst_shifted=" << st.worst_shifted
                << "\n";
        } else {
            std::cout
                << "NO_SAT_FOUND"
                << " strict=0"
                << " margin=" << (st.margin_ok ? 1 : 0)
                << " worst_raw=" << st.worst_raw
                << " worst_shifted=" << st.worst_shifted
                << "\n";
        }

        std::cout
            << "SUMMARY"
            << " n=" << o.n
            << " gens=" << gens.size()
            << " iter=" << iter_done
            << " max_iter=" << (unlimited_steps ? "inf" : std::to_string(max_steps_limit))
            << " V=" << final_val
            << " GRAD=" << final_grad
            << " STEP=" << step
            << " STRICT=" << (st.strict_ok ? 1 : 0)
            << " MARGIN=" << (st.margin_ok ? 1 : 0)
            << " worst_raw=" << st.worst_raw
            << " worst_shifted=" << st.worst_shifted
            << " viol_strict=" << st.strict_violations
            << " viol_margin=" << st.margin_violations
            << " edge_ratio=" << gs.global_edge_ratio
            << " max_abs_x=" << gs.max_abs_x_intersection
            << " reg_scale=" << g_reg_runtime_scale
            << " sat_hits=" << sat_hits
            << " best_sat_edge_ratio=" << best_sat_edge_ratio_out
            << " best_V=" << best_result
            << "\n";
        std::cout << "#RESULT_END\n";
    }

    std::cout << "#LINES_BEGIN\n";
    terms.print_tuples(std::cout);
    std::cout << "#LINES_END\n";

    return 0;
}
