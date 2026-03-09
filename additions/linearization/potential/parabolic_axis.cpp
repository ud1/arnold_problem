#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "interfaces/highs_c_api.h"

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    std::vector<int> parallels;
    size_t n = 0;

    int norm(int value) const {
        const int nn = (int)n;
        return (value + 2 * nn) % nn;
    }

    int norm2(int value) const {
        const int nn = (int)n;
        return (value + 4 * nn) % (2 * nn);
    }

    bool can_rotate(int rotation) const {
        if (rotation < 0 || rotation >= 2 * (int)n) return false;
        int rotated = norm(rotation);
        int p = parallels[(size_t)rotated] >= 0 ? parallels[(size_t)rotated] : rotated;
        return p >= rotated;
    }

    int rotate_line_num(int rotation, int line) const {
        if (line >= rotation) return line - rotation;
        return norm(line - rotation);
    }

    int get_line_len(int rotation, int i) const {
        int line = i + rotation;
        if (line < (int)n) {
            return (int)intersections[(size_t)line].size();
        }
        line = norm(line);
        if (parallels[(size_t)line] >= 0) line = parallels[(size_t)line];
        return (int)intersections[(size_t)line].size();
    }

    int get_val(int rotation, int i, int j) const {
        int line = norm2(i + rotation);
        int v = -1;
        if (line < (int)n) {
            v = (int)intersections[(size_t)line][(size_t)j];
        } else {
            line = norm(line);
            if (parallels[(size_t)line] >= 0) line = parallels[(size_t)line];
            const auto& row = intersections[(size_t)line];
            v = (int)row[row.size() - 1 - (size_t)j];
        }

        if ((rotation < (int)n && v >= rotation) ||
            (rotation >= (int)n && v < norm(rotation))) {
            return norm(v - rotation);
        }

        if (parallels[(size_t)v] >= 0) v = parallels[(size_t)v];
        return norm(v - rotation);
    }

    bool rotate(int rotation, OMatrix& out) const {
        if (n == 0) return false;
        int rot = rotation % (2 * (int)n);
        if (rot < 0) rot += 2 * (int)n;
        if (rot == 0) {
            out = *this;
            return true;
        }
        if (!can_rotate(rot)) return false;

        OMatrix r;
        r.n = n;
        r.intersections.assign(n, {});
        r.parallels.assign(n, -1);

        for (size_t i = 0; i < n; ++i) {
            int len = get_line_len(rot, (int)i);
            r.intersections[i].reserve((size_t)len);
            for (int j = 0; j < len; ++j) {
                r.intersections[i].push_back((size_t)get_val(rot, (int)i, j));
            }
        }

        for (size_t k = 0; k < n; ++k) {
            int v = parallels[k];
            if (v < 0) continue;
            int nk = rotate_line_num(rot, (int)k);
            int nv = rotate_line_num(rot, v);
            r.parallels[(size_t)nk] = nv;
        }

        out = std::move(r);
        return true;
    }

    bool has_parallels() const {
        for (int p : parallels) {
            if (p >= 0) return true;
        }
        return false;
    }

    bool sphere_rotate(size_t val, OMatrix& out) const {
        if (n == 0) return false;
        if (n % 2 == 0) return false;
        if (val >= n) return false;
        if (has_parallels()) return false;
        if (intersections[val].size() != n - 1) return false;
        for (size_t i = 0; i < n; ++i) {
            if (intersections[i].size() != n - 1) return false;
        }

        const size_t ext = n;
        std::vector<int> reord(n + 1, -1);
        for (size_t i = 0; i < n - 1; ++i) {
            size_t line = intersections[val][i];
            if (line >= n) return false;
            if (line == val) return false;
            if (reord[line] >= 0) return false;
            reord[line] = (int)i;
        }
        reord[ext] = (int)(n - 1);

        for (size_t i = 0; i < n; ++i) {
            if (i != val && reord[i] < 0) return false;
        }

        std::vector<std::vector<size_t>> temp(n + 1);
        for (size_t i = 0; i < n; ++i) {
            temp[i] = intersections[i];
            temp[i].push_back(ext);
        }
        temp[ext].resize(n);
        for (size_t i = 0; i < n; ++i) temp[ext][i] = i;

        OMatrix r;
        r.n = n;
        r.intersections.assign(n, {});
        r.parallels.assign(n, -1);

        for (size_t i = 0; i < n; ++i) {
            size_t v = temp[val][i];
            if (v >= temp.size()) return false;
            const auto& temp_line = temp[v];
            if (temp_line.size() != n) return false;

            size_t val_pos = temp_line.size();
            for (size_t j = 0; j < temp_line.size(); ++j) {
                if (temp_line[j] == val) {
                    val_pos = j;
                    break;
                }
            }
            if (val_pos == temp_line.size()) return false;

            auto& new_line = r.intersections[i];
            new_line.reserve(n - 1);

            if (v < val) {
                for (size_t j = val_pos; j > 0; --j) {
                    size_t old = temp_line[j - 1];
                    if (old >= reord.size()) return false;
                    int mapped = reord[old];
                    if (mapped < 0) return false;
                    new_line.push_back((size_t)mapped);
                }
                for (size_t j = n; j > val_pos + 1; --j) {
                    size_t old = temp_line[j - 1];
                    if (old >= reord.size()) return false;
                    int mapped = reord[old];
                    if (mapped < 0) return false;
                    new_line.push_back((size_t)mapped);
                }
            } else {
                for (size_t j = val_pos + 1; j < n; ++j) {
                    size_t old = temp_line[j];
                    if (old >= reord.size()) return false;
                    int mapped = reord[old];
                    if (mapped < 0) return false;
                    new_line.push_back((size_t)mapped);
                }
                for (size_t j = 0; j < val_pos; ++j) {
                    size_t old = temp_line[j];
                    if (old >= reord.size()) return false;
                    int mapped = reord[old];
                    if (mapped < 0) return false;
                    new_line.push_back((size_t)mapped);
                }
            }

            if (new_line.size() != n - 1) return false;
        }

        out = std::move(r);
        return true;
    }
};

static bool validate_omatrix(const OMatrix& o, bool require_full_rows = false) {
    if (o.intersections.size() != o.n) return false;
    if (o.parallels.size() != o.n) return false;

    std::vector<std::vector<unsigned char>> seen(o.n, std::vector<unsigned char>(o.n, 0));
    for (size_t i = 0; i < o.n; ++i) {
        int p = o.parallels[i];
        if (p < -1 || p >= (int)o.n) return false;
        if (p >= 0) {
            if ((size_t)p == i) return false;
            if (o.parallels[(size_t)p] != (int)i) return false;
        }

        const auto& row = o.intersections[i];
        if (require_full_rows && row.size() != o.n - 1) return false;
        if (row.size() > o.n - 1) return false;
        for (size_t v : row) {
            if (v >= o.n) return false;
            if (v == i) return false;
            if (seen[i][v]) return false;
            seen[i][v] = 1;
        }
    }

    for (size_t i = 0; i < o.n; ++i) {
        for (size_t j = i + 1; j < o.n; ++j) {
            if (seen[i][j] != seen[j][i]) return false;
        }
    }
    return true;
}

static OMatrix make_omatrix(const std::vector<size_t>& gens) {
    if (gens.empty()) {
        throw std::runtime_error("Empty generator list");
    }

    const size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);

    std::vector<std::vector<size_t>> intersections(n);
    for (size_t gen : gens) {
        if (gen + 1 >= n) {
            throw std::runtime_error("Generator index out of range");
        }
        size_t first = lines[gen];
        size_t second = lines[gen + 1];
        intersections[first].push_back(second);
        intersections[second].push_back(first);
        lines[gen] = second;
        lines[gen + 1] = first;
    }

    std::vector<int> parallels(n, -1);
    for (size_t i = 0; i + 1 < n; ++i) {
        if (lines[i] < lines[i + 1]) {
            parallels[lines[i]] = (int)lines[i + 1];
            parallels[lines[i + 1]] = (int)lines[i];
        }
    }

    OMatrix out{intersections, parallels, n};
    if (!validate_omatrix(out, false)) {
        throw std::runtime_error("Invalid O-matrix built from generators");
    }
    return out;
}

struct AxisModeConfig {
    bool boundary_pair = true;
    size_t left = 0;
    size_t right = 0;
    long double eps = 1e-3L;
};

struct BuildResult {
    std::vector<long double> m;
    size_t axis_left = 0;
    size_t axis_right = 0;
};

static BuildResult build_fixed_slopes(size_t n, const AxisModeConfig& cfg) {
    if (n < 2) {
        throw std::runtime_error("Axis mode requires at least 2 lines");
    }
    if (!(cfg.eps > 0.0L) || !std::isfinite((double)cfg.eps)) {
        throw std::runtime_error("axis eps must be finite and > 0");
    }

    const long double pi = acosl(-1.0L);
    const long double half_pi = pi * 0.5L;
    const long double delta = pi / (2.0L * (long double)(n - 1));

    std::vector<long double> m(n, 0.0L);
    size_t axis_left = 0;
    size_t axis_right = n - 1;

    if (cfg.boundary_pair) {
        const long double eps_max = pi / (long double)(n - 1);
        if (cfg.eps >= eps_max) {
            throw std::runtime_error("boundary axis mode requires eps < pi/(n-1)");
        }

        for (size_t i = 0; i < n; ++i) {
            long double theta = 0.0L;
            if (i == axis_left) {
                theta = -half_pi + cfg.eps;
            } else if (i == axis_right) {
                theta = half_pi - cfg.eps;
            } else {
                size_t mid_i = i - 1;
                long double q = (long double)(2 * mid_i) - (long double)(n - 3);
                theta = q * delta;
            }
            if (fabsl(theta) >= half_pi - 1e-15L) {
                throw std::runtime_error("axis placement reaches pi/2; choose smaller eps");
            }
            m[i] = tanl(theta);
        }
    } else {
        if (cfg.right != cfg.left + 1) {
            throw std::runtime_error("axis pair must be adjacent: J=I+1");
        }
        if (cfg.right >= n) {
            throw std::runtime_error("axis pair index out of range");
        }
        if (cfg.eps >= 2.0L * delta) {
            throw std::runtime_error("axis eps must be smaller than pi/(n-1)");
        }

        axis_left = cfg.left;
        axis_right = cfg.right;
        const long double q_axis = (long double)(2 * axis_left) - (long double)(n - 2);
        const long double theta_axis = q_axis * delta;

        for (size_t i = 0; i < n; ++i) {
            long double theta = 0.0L;
            if (i == axis_left) {
                theta = theta_axis - cfg.eps;
            } else if (i == axis_right) {
                theta = theta_axis + cfg.eps;
            } else {
                size_t dir_i = (i < axis_left) ? i : (i - 1);
                long double q = (long double)(2 * dir_i) - (long double)(n - 2);
                theta = q * delta;
            }
            if (fabsl(theta) >= half_pi - 1e-15L) {
                throw std::runtime_error("axis placement reaches pi/2; choose different axis pair/eps");
            }
            m[i] = tanl(theta);
        }
    }

    return BuildResult{m, axis_left, axis_right};
}

struct EdgeSpec {
    size_t b1, b2, m1, m2, b3, b4, m3, m4;
};

static std::vector<EdgeSpec> build_edge_specs(const OMatrix& o) {
    std::vector<EdgeSpec> specs;

    auto push_spec = [&](size_t b1, size_t b2, size_t m1, size_t m2, size_t b3, size_t b4, size_t m3, size_t m4) {
        specs.push_back(EdgeSpec{b1, b2, m1, m2, b3, b4, m3, m4});
    };

    for (size_t i = 0; i < o.n; ++i) {
        const auto& line = o.intersections[i];
        if (line.size() < 2) continue;

        for (size_t idx = 0; idx + 1 < line.size(); ++idx) {
            size_t j = line[idx];
            size_t k = line[idx + 1];
            if (j >= o.n || k >= o.n) continue;
            if (j == i || k == i) continue;

            if (i > j && i > k) {
                push_spec(k, i, i, j, j, i, i, k);
            } else if (i > j && i < k) {
                push_spec(i, k, i, j, j, i, k, i);
            } else if (i < j && i > k) {
                push_spec(k, i, j, i, i, j, i, k);
            } else {
                push_spec(i, k, j, i, i, j, k, i);
            }
        }
    }

    return specs;
}

struct LinearSystem {
    std::vector<std::vector<long double>> A;
    std::vector<long double> rhs;
    std::vector<int> var_index;
};

static LinearSystem build_linear_system(
    const OMatrix& o,
    const std::vector<long double>& m,
    size_t axis_left,
    size_t axis_right,
    long double strict_margin
) {
    const auto specs = build_edge_specs(o);
    std::vector<int> var_index(o.n, -1);
    size_t d = 0;
    for (size_t i = 0; i < o.n; ++i) {
        if (i == axis_left || i == axis_right) continue;
        var_index[i] = (int)d++;
    }

    std::vector<std::vector<long double>> A;
    std::vector<long double> rhs;
    A.reserve(specs.size());
    rhs.reserve(specs.size());

    for (const auto& s : specs) {
        long double c1 = m[s.m1] - m[s.m2];
        long double c2 = m[s.m3] - m[s.m4];

        std::vector<long double> row(d, 0.0L);
        long double constant = 0.0L;

        auto add_coeff = [&](size_t line, long double coeff) {
            int vi = var_index[line];
            if (vi >= 0) {
                row[(size_t)vi] += coeff;
            } else {
                constant += coeff * 0.0L; // anchored b = 0
            }
        };

        add_coeff(s.b1, +c1);
        add_coeff(s.b2, -c1);
        add_coeff(s.b3, -c2);
        add_coeff(s.b4, +c2);

        long double row_scale = 0.0L;
        for (long double v : row) row_scale = std::max(row_scale, fabsl(v));
        long double r = -strict_margin - constant;

        if (row_scale == 0.0L) {
            if (!(0.0L <= r)) {
                A.clear();
                rhs.clear();
                A.push_back(std::vector<long double>(d, 0.0L));
                rhs.push_back(-1.0L);
                return LinearSystem{A, rhs, var_index};
            }
            continue;
        }

        for (long double& v : row) v /= row_scale;
        r /= row_scale;
        A.push_back(std::move(row));
        rhs.push_back(r);
    }

    return LinearSystem{A, rhs, var_index};
}

struct SolveResult {
    bool feasible = false;
    long double max_violation = std::numeric_limits<long double>::infinity();
    long double t = std::numeric_limits<long double>::infinity();
    size_t newton_steps = 0;
    std::vector<long double> x;
};

static SolveResult solve_feasibility_lp(const LinearSystem& sys) {
    SolveResult out;

    const size_t m = sys.A.size();
    const size_t d = sys.var_index.empty() ? 0 : *std::max_element(sys.var_index.begin(), sys.var_index.end()) + 1;

    if (m == 0) {
        out.feasible = true;
        out.max_violation = 0.0L;
        out.t = 0.0L;
        out.x.assign(d, 0.0L);
        return out;
    }

    const double inf = 1.0e30;
    const size_t t_col = d;
    const size_t num_vars = d + 1; // decision vars x plus phase-I slack t
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
        row_upper[i] = (double)sys.rhs[i];
        for (size_t j = 0; j < d; ++j) {
            long double v = sys.A[i][j];
            if (v == 0.0L) continue;
            a_index.push_back((HighsInt)j);
            a_value.push_back((double)v);
        }
        // A x - t <= rhs
        a_index.push_back((HighsInt)t_col);
        a_value.push_back(-1.0);
    }
    a_start[m] = (HighsInt)a_index.size();

    std::vector<double> col_value(num_vars, 0.0), col_dual(num_vars, 0.0);
    std::vector<double> row_value(m, 0.0), row_dual(m, 0.0);
    std::vector<HighsInt> col_basis_status(num_vars, 0), row_basis_status(m, 0);

    HighsInt model_status = kHighsModelStatusNotset;
    HighsInt run_status = Highs_lpCall(
        num_col,
        num_row,
        (HighsInt)a_index.size(),
        kHighsMatrixFormatRowwise,
        kHighsObjSenseMinimize,
        0.0,
        col_cost.data(),
        col_lower.data(),
        col_upper.data(),
        m ? row_lower.data() : nullptr,
        m ? row_upper.data() : nullptr,
        m ? a_start.data() : nullptr,
        a_index.empty() ? nullptr : a_index.data(),
        a_value.empty() ? nullptr : a_value.data(),
        col_value.data(),
        col_dual.data(),
        m ? row_value.data() : nullptr,
        m ? row_dual.data() : nullptr,
        col_basis_status.data(),
        m ? row_basis_status.data() : nullptr,
        &model_status
    );

    std::vector<long double> x(d, 0.0L);
    for (size_t j = 0; j < d; ++j) x[j] = (long double)col_value[j];
    const long double t_value = (long double)col_value[t_col];

    long double max_violation = -std::numeric_limits<long double>::infinity();
    for (size_t i = 0; i < m; ++i) {
        long double ax = 0.0L;
        for (size_t j = 0; j < d; ++j) ax += sys.A[i][j] * x[j];
        max_violation = std::max(max_violation, ax - sys.rhs[i]);
    }

    out.x = std::move(x);
    out.max_violation = max_violation;
    out.t = t_value;
    out.newton_steps = 0;
    out.feasible = (run_status == kHighsStatusOk &&
                    model_status == kHighsModelStatusOptimal &&
                    out.t <= 1e-10L &&
                    out.max_violation <= 1e-10L);
    return out;
}

struct AttemptResult {
    bool sat = false;
    int sphere_rotation = -1;
    int rotation = 0;
    size_t n = 0;
    size_t gens_count = 0;
    size_t axis_left = 0;
    size_t axis_right = 0;
    long double axis_eps = 0.0L;
    long double margin_target = 0.0L;
    long double t = std::numeric_limits<long double>::infinity();
    long double max_violation_lp = std::numeric_limits<long double>::infinity();
    long double worst_raw = std::numeric_limits<long double>::infinity();
    std::vector<long double> m;
    std::vector<long double> b;
    std::vector<size_t> omatrix_gens;
};

static std::vector<size_t> omatrix_to_generators(const OMatrix& o) {
    const size_t n = o.n;
    if (n < 2) return {};

    size_t incidences = 0;
    for (const auto& row : o.intersections) incidences += row.size();
    if (incidences % 2 != 0) {
        throw std::runtime_error("Invalid O-matrix: odd incidence count");
    }
    const size_t steps = incidences / 2;

    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);
    std::vector<size_t> pos(n, 0);
    std::vector<size_t> gens(steps, 0);

    for (size_t step = 0; step < steps; ++step) {
        bool found = false;
        for (size_t gen = 0; gen + 1 < n; ++gen) {
            size_t left = lines[gen];
            size_t right = lines[gen + 1];
            if (pos[left] >= o.intersections[left].size()) continue;
            if (pos[right] >= o.intersections[right].size()) continue;
            if (o.intersections[left][pos[left]] != right) continue;
            if (o.intersections[right][pos[right]] != left) continue;

            ++pos[left];
            ++pos[right];
            std::swap(lines[gen], lines[gen + 1]);
            gens[step] = gen;
            found = true;
            break;
        }
        if (!found) {
            throw std::runtime_error("Invalid O-matrix: cannot convert to generators");
        }
    }

    for (size_t line = 0; line < n; ++line) {
        if (pos[line] != o.intersections[line].size()) {
            throw std::runtime_error("Invalid O-matrix: unconsumed intersections");
        }
    }

    return gens;
}

static AttemptResult solve_for_omatrix(
    const OMatrix& o,
    size_t gens_count,
    const AxisModeConfig& axis,
    long double strict_margin,
    int rotation
) {
    if (!validate_omatrix(o, false)) {
        throw std::runtime_error("Invalid O-matrix before solve");
    }

    BuildResult br = build_fixed_slopes(o.n, axis);
    LinearSystem sys = build_linear_system(o, br.m, br.axis_left, br.axis_right, strict_margin);
    SolveResult sr = solve_feasibility_lp(sys);

    std::vector<long double> b(o.n, 0.0L);
    for (size_t line = 0; line < o.n; ++line) {
        if (line == br.axis_left || line == br.axis_right) continue;
        int vi = sys.var_index[line];
        if (vi >= 0 && (size_t)vi < sr.x.size()) b[line] = sr.x[(size_t)vi];
    }

    long double worst_raw = -std::numeric_limits<long double>::infinity();
    auto specs = build_edge_specs(o);
    for (const auto& s : specs) {
        long double c1 = br.m[s.m1] - br.m[s.m2];
        long double c2 = br.m[s.m3] - br.m[s.m4];
        long double raw = (b[s.b1] - b[s.b2]) * c1 - (b[s.b3] - b[s.b4]) * c2;
        worst_raw = std::max(worst_raw, raw);
    }
    if (specs.empty()) worst_raw = 0.0L;

    AttemptResult out;
    out.sat = (sr.feasible && worst_raw < -1e-10L);
    out.rotation = rotation;
    out.n = o.n;
    out.gens_count = gens_count;
    out.axis_left = br.axis_left;
    out.axis_right = br.axis_right;
    out.axis_eps = axis.eps;
    out.margin_target = strict_margin;
    out.t = sr.t;
    out.max_violation_lp = sr.max_violation;
    out.worst_raw = worst_raw;
    out.m = std::move(br.m);
    out.b = std::move(b);
    out.omatrix_gens = omatrix_to_generators(o);
    return out;
}

static AttemptResult solve_case_with_rotations(
    const OMatrix& base,
    size_t gens_count,
    const AxisModeConfig& axis,
    long double strict_margin,
    bool all_rotations,
    int& tried_rotations
) {
    AttemptResult best;
    bool have_best = false;
    tried_rotations = 0;

    const int max_rotation = all_rotations ? 2 * (int)base.n : 1;
    for (int rot = 0; rot < max_rotation; ++rot) {
        OMatrix current;
        if (!base.rotate(rot, current)) continue;
        if (!validate_omatrix(current, false)) continue;
        ++tried_rotations;

        AttemptResult cur = solve_for_omatrix(current, gens_count, axis, strict_margin, rot);

        if (!have_best) {
            best = std::move(cur);
            have_best = true;
            continue;
        }

        if (cur.sat && !best.sat) {
            best = std::move(cur);
            continue;
        }

        if (cur.sat == best.sat && cur.worst_raw < best.worst_raw) {
            best = std::move(cur);
        }
    }

    if (!have_best) {
        throw std::runtime_error("No valid O-matrix rotations available");
    }
    return best;
}

static AttemptResult solve_case_with_sphere_and_rotations(
    const OMatrix& base,
    size_t gens_count,
    const AxisModeConfig& axis,
    long double strict_margin,
    bool all_rotations,
    bool sphere_rotations,
    int& tried_sphere_rotations,
    int& tried_plane_rotations
) {
    tried_sphere_rotations = 0;
    tried_plane_rotations = 0;

    AttemptResult best;
    bool have_best = false;

    const int sphere_count = sphere_rotations ? (int)base.n : 1;
    for (int s = 0; s < sphere_count; ++s) {
        OMatrix current_base;
        int sphere_tag = -1;
        if (sphere_rotations) {
            if (!base.sphere_rotate((size_t)s, current_base)) continue;
            if (!validate_omatrix(current_base, true)) continue;
            sphere_tag = s;
        } else {
            current_base = base;
            if (!validate_omatrix(current_base, false)) continue;
        }
        ++tried_sphere_rotations;

        int local_tried_rotations = 0;
        AttemptResult cur = solve_case_with_rotations(
            current_base, gens_count, axis, strict_margin, all_rotations, local_tried_rotations
        );
        cur.sphere_rotation = sphere_tag;
        tried_plane_rotations += local_tried_rotations;

        if (!have_best) {
            best = std::move(cur);
            have_best = true;
            continue;
        }
        if (cur.sat && !best.sat) {
            best = std::move(cur);
            continue;
        }
        if (cur.sat == best.sat && cur.worst_raw < best.worst_raw) {
            best = std::move(cur);
        }
    }

    if (!have_best) {
        throw std::runtime_error("No valid sphere/plane rotations available");
    }
    return best;
}

static std::vector<size_t> parse_gens_numbers(const std::string& s) {
    std::vector<size_t> out;
    std::stringstream ss(s);
    size_t val = 0;
    while (ss >> val) out.push_back(val);
    return out;
}

static std::string trim_copy(const std::string& s) {
    size_t l = 0;
    while (l < s.size() && std::isspace((unsigned char)s[l])) ++l;
    size_t r = s.size();
    while (r > l && std::isspace((unsigned char)s[r - 1])) --r;
    return s.substr(l, r - l);
}

static bool is_numbers_only_line(const std::string& s) {
    bool has_digit = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) {
            has_digit = true;
            continue;
        }
        if (!std::isspace((unsigned char)c)) return false;
    }
    return has_digit;
}

static std::vector<size_t> extract_nonnegative_ints(const std::string& s) {
    std::vector<size_t> out;
    unsigned long long cur = 0;
    bool in_num = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) {
            in_num = true;
            cur = cur * 10ULL + (unsigned long long)(c - '0');
        } else if (in_num) {
            out.push_back((size_t)cur);
            cur = 0;
            in_num = false;
        }
    }
    if (in_num) out.push_back((size_t)cur);
    return out;
}

static std::vector<std::vector<size_t>> parse_filter_cases_from_stream(std::istream& in) {
    std::vector<std::vector<size_t>> cases;
    std::vector<size_t> current;
    bool header_case_active = false;
    std::string line;

    while (std::getline(in, line)) {
        size_t close_paren_pos = line.find(')');
        if (close_paren_pos != std::string::npos) {
            if (!current.empty()) {
                cases.push_back(current);
                current.clear();
            }
            header_case_active = true;
            std::string tail = line.substr(close_paren_pos + 1);
            auto nums = extract_nonnegative_ints(tail);
            current.insert(current.end(), nums.begin(), nums.end());
            continue;
        }

        if (header_case_active) {
            auto nums = extract_nonnegative_ints(line);
            current.insert(current.end(), nums.begin(), nums.end());
            continue;
        }

        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) continue;
        if (is_numbers_only_line(trimmed)) {
            auto nums = extract_nonnegative_ints(trimmed);
            if (!nums.empty()) cases.push_back(std::move(nums));
        }
    }

    if (!current.empty()) cases.push_back(current);
    return cases;
}

static bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static bool parse_axis_pair_spec(const std::string& s, size_t& left, size_t& right) {
    std::vector<size_t> nums;
    unsigned long long cur = 0;
    bool in_num = false;
    for (char c : s) {
        if (std::isdigit((unsigned char)c)) {
            in_num = true;
            cur = cur * 10ULL + (unsigned long long)(c - '0');
        } else if (in_num) {
            nums.push_back((size_t)cur);
            cur = 0;
            in_num = false;
        }
    }
    if (in_num) nums.push_back((size_t)cur);

    if (nums.size() != 2) return false;
    left = nums[0];
    right = nums[1];
    if (left > right) std::swap(left, right);
    return true;
}

static void print_generators_line(const std::vector<size_t>& gens, const std::string& prefix = "GENS") {
    std::cout << prefix << " [";
    for (size_t i = 0; i < gens.size(); ++i) {
        if (i) std::cout << " ";
        std::cout << gens[i];
    }
    std::cout << "]\n";
}

static void print_lines_csv_block(const std::vector<long double>& m, const std::vector<long double>& b) {
    std::cout << "#LINES_BEGIN\n";
    std::cout << "m,b\n";
    for (size_t i = 0; i < m.size(); ++i) {
        std::cout << (double)m[i] << "," << (double)b[i] << "\n";
    }
    std::cout << "#LINES_END\n";
}

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\" [--axis-pair [I,J]] [--axis-eps E] [--all-rotations] [--sphere-rotations] [--print-gens]\n"
        << "  " << argv0 << " --gens-file path.txt [--axis-pair [I,J]] [--axis-eps E] [--all-rotations] [--sphere-rotations] [--print-gens]\n"
        << "  cat input.txt | " << argv0 << " [--axis-pair [I,J]] [--axis-eps E] [--all-rotations] [--sphere-rotations] [--print-gens]\n"
        << "\n"
        << "Only axis-pair mode is supported.\n"
        << "  --axis-pair without I,J: anchor boundary lines 0 and n-1\n"
        << "  --axis-pair I,J: anchor adjacent lines I and J\n"
        << "  --all-rotations: iterate all valid O-matrix plane rotations\n"
        << "  --sphere-rotations: iterate sphere rotations; implies --all-rotations\n"
        << "  --print-gens: print generators for the O-matrix used to produce LINES\n";
}

int main(int argc, char** argv) {
    try {
        std::string gens_str;
        bool have_gens = false;
        bool filter_mode = false;

        AxisModeConfig axis;
        bool axis_pair_seen = false;
        bool all_rotations = false;
        bool sphere_rotations = false;
        bool print_gens = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            }
            if (arg == "--gens") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --gens");
                gens_str = argv[++i];
                have_gens = true;
                continue;
            }
            if (arg == "--gens-file") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --gens-file");
                if (!read_file(argv[++i], gens_str)) throw std::runtime_error("Failed to read gens file");
                have_gens = true;
                continue;
            }
            if (arg == "--axis-pair") {
                axis_pair_seen = true;
                axis.boundary_pair = true;
                if (i + 1 < argc) {
                    std::string next = argv[i + 1];
                    if (!next.empty() && next[0] != '-' && next.find(',') != std::string::npos) {
                        size_t l = 0, r = 0;
                        if (!parse_axis_pair_spec(next, l, r)) {
                            throw std::runtime_error("Invalid --axis-pair format, expected I,J");
                        }
                        axis.boundary_pair = false;
                        axis.left = l;
                        axis.right = r;
                        ++i;
                    }
                }
                continue;
            }
            if (arg == "--axis-eps") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --axis-eps");
                axis.eps = std::stold(argv[++i]);
                continue;
            }
            if (arg == "--all-rotations") {
                all_rotations = true;
                continue;
            }
            if (arg == "--sphere-rotations") {
                sphere_rotations = true;
                all_rotations = true;
                continue;
            }
            if (arg == "--print-gens") {
                print_gens = true;
                continue;
            }
            if (!arg.empty() && arg[0] != '-') {
                std::ostringstream ss;
                ss << arg;
                for (int j = i + 1; j < argc; ++j) ss << ' ' << argv[j];
                gens_str = ss.str();
                have_gens = true;
                break;
            }
            throw std::runtime_error("Unknown option: " + arg);
        }

        if (!have_gens && !isatty(STDIN_FILENO)) {
            filter_mode = true;
        }
        if (!axis_pair_seen) {
            axis_pair_seen = true;
            axis.boundary_pair = true;
        }
        if (all_rotations && !axis.boundary_pair) {
            throw std::runtime_error("--all-rotations supports only boundary axis mode");
        }
        if (sphere_rotations && !axis.boundary_pair) {
            throw std::runtime_error("--sphere-rotations supports only boundary axis mode");
        }

        const long double strict_margin = 1.0L;

        if (filter_mode) {
            auto cases = parse_filter_cases_from_stream(std::cin);
            if (cases.empty()) throw std::runtime_error("No cases parsed from stdin");

            for (size_t ci = 0; ci < cases.size(); ++ci) {
                const auto& gens = cases[ci];
                if (gens.empty()) continue;
                pid_t pid = fork();
                if (pid < 0) {
                    throw std::runtime_error("fork() failed in filter mode");
                }

                if (pid == 0) {
                    int devnull = open("/dev/null", O_WRONLY);
                    if (devnull >= 0) {
                        (void)dup2(devnull, STDERR_FILENO);
                        close(devnull);
                    }
                    try {
                        OMatrix o = make_omatrix(gens);
                        int tried_sphere_rotations = 0;
                        int tried_plane_rotations = 0;
                        AttemptResult res = solve_case_with_sphere_and_rotations(
                            o, gens.size(), axis, strict_margin, all_rotations, sphere_rotations,
                            tried_sphere_rotations, tried_plane_rotations
                        );

                        std::cout << (res.sat ? "SAT" : "NO_SOLUTION")
                                  << " #" << (ci + 1)
                                  << " n=" << res.n
                                  << " gens=" << gens.size()
                                  << " sphere_rotation=" << res.sphere_rotation
                                  << " rotation=" << res.rotation
                                  << " tried_sphere_rotations=" << tried_sphere_rotations
                                  << " tried_plane_rotations=" << tried_plane_rotations
                                  << " margin_target=" << (double)strict_margin
                                  << " worst_raw=" << std::setprecision(18) << (double)res.worst_raw
                                  << " t=" << (double)res.t
                                  << "\n";
                        if (print_gens) {
                            print_generators_line(res.omatrix_gens, "GENS #" + std::to_string(ci + 1));
                        }
                        std::cout.flush();
                        _exit(0);
                    } catch (const std::exception& e) {
                        std::cout << "ERROR"
                                  << " #" << (ci + 1)
                                  << " message=" << e.what()
                                  << "\n";
                        std::cout.flush();
                        _exit(0);
                    }
                }

                int status = 0;
                if (waitpid(pid, &status, 0) < 0) {
                    throw std::runtime_error("waitpid() failed in filter mode");
                }
            }
            return 0;
        }

        if (!have_gens) throw std::runtime_error("No generators provided");
        auto gens = parse_gens_numbers(gens_str);
        if (gens.empty()) throw std::runtime_error("No valid generator numbers parsed");

        OMatrix o = make_omatrix(gens);
        int tried_sphere_rotations = 0;
        int tried_plane_rotations = 0;
        AttemptResult res = solve_case_with_sphere_and_rotations(
            o, gens.size(), axis, strict_margin, all_rotations, sphere_rotations,
            tried_sphere_rotations, tried_plane_rotations
        );

        std::cout << std::setprecision(18);
        if (res.sat) {
            std::cout << "SAT\n";
        } else {
            std::cout << "NO_SOLUTION\n";
        }

        std::cout << "n=" << res.n
                  << " gens=" << res.gens_count
                  << " sphere_rotation=" << res.sphere_rotation
                  << " rotation=" << res.rotation
                  << " tried_sphere_rotations=" << tried_sphere_rotations
                  << " tried_plane_rotations=" << tried_plane_rotations
                  << " axis_left=" << res.axis_left
                  << " axis_right=" << res.axis_right
                  << " axis_eps=" << (double)res.axis_eps
                  << " margin_target=" << (double)strict_margin
                  << " newton=0"
                  << " t=" << (double)res.t
                  << " max_violation_lp=" << (double)res.max_violation_lp
                  << " worst_raw=" << (double)res.worst_raw
                  << "\n";

        print_lines_csv_block(res.m, res.b);
        if (print_gens) {
            print_generators_line(res.omatrix_gens);
        }

        return res.sat ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
