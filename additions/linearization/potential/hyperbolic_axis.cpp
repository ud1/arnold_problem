#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "interfaces/highs_c_api.h"

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    std::vector<int> parallels;
    size_t n = 0;
    size_t num_vertices = 0;

    int norm(int value) const {
        const int nn = (int)n;
        return (value + 2 * nn) % nn;
    }

    int norm2(int value) const {
        const int nn = (int)n;
        return (value + 4 * nn) % (2 * nn);
    }

    bool can_rotate(int rotation) const {
        if (rotation < 0 || rotation >= 2 * (int)n) {
            return false;
        }
        int rotated = norm(rotation);
        int p = parallels[rotated] >= 0 ? parallels[rotated] : rotated;
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
        if (parallels[(size_t)line] >= 0) {
            line = parallels[(size_t)line];
        }
        return (int)intersections[(size_t)line].size();
    }

    int get_val(int rotation, int i, int j) const {
        int line = norm2(i + rotation);
        int v = -1;
        if (line < (int)n) {
            v = (int)intersections[(size_t)line][(size_t)j];
        } else {
            line = norm(line);
            if (parallels[(size_t)line] >= 0) {
                line = parallels[(size_t)line];
            }
            const auto& row = intersections[(size_t)line];
            v = (int)row[row.size() - 1 - (size_t)j];
        }

        if ((rotation < (int)n && v >= rotation) ||
            (rotation >= (int)n && v < norm(rotation))) {
            return norm(v - rotation);
        }

        if (parallels[(size_t)v] >= 0) {
            v = parallels[(size_t)v];
        }
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
        r.num_vertices = num_vertices;
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

    OMatrix reflect() const {
        OMatrix r;
        r.n = n;
        r.num_vertices = num_vertices;
        r.intersections.assign(n, {});
        r.parallels.assign(n, -1);

        for (size_t old = 0; old < n; ++old) {
            size_t ni = n - 1 - old;
            const auto& row = intersections[old];
            auto& out_row = r.intersections[ni];
            out_row.reserve(row.size());
            for (size_t v : row) out_row.push_back(n - 1 - v);
        }

        for (size_t k = 0; k < n; ++k) {
            if (parallels[k] < 0) continue;
            int nk = (int)(n - 1 - k);
            int nv = (int)(n - 1 - (size_t)parallels[k]);
            r.parallels[(size_t)nk] = nv;
        }

        return r;
    }
};

static OMatrix make_omatrix(const std::vector<size_t>& gens) {
    if (gens.empty()) throw std::runtime_error("Empty generator list");

    const size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);

    std::vector<std::vector<size_t>> intersections(n);
    for (size_t gen : gens) {
        if (gen + 1 >= n) throw std::runtime_error("Generator index out of range");
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

    return OMatrix{intersections, parallels, n, gens.size()};
}

struct EdgeSpec {
    size_t b1, b2, m1, m2, b3, b4, m3, m4;
};

static std::vector<EdgeSpec> build_edge_specs(const OMatrix& o) {
    std::vector<EdgeSpec> specs;
    for (size_t i = 0; i < o.n; ++i) {
        const auto& line = o.intersections[i];
        if (line.size() < 2) continue;

        for (size_t idx = 0; idx + 1 < line.size(); ++idx) {
            size_t j = line[idx];
            size_t k = line[idx + 1];

            if (i > j && i > k) {
                specs.push_back({k, i, i, j, j, i, i, k});
            } else if (i > j && i < k) {
                specs.push_back({i, k, i, j, j, i, k, i});
            } else if (i < j && i > k) {
                specs.push_back({k, i, j, i, i, j, i, k});
            } else {
                specs.push_back({i, k, j, i, i, j, k, i});
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

static LinearSystem build_linear_system_for_m(
    const OMatrix& o,
    const std::vector<long double>& b,
    size_t axis_line,
    long double margin,
    bool m_order_increasing,
    long double m_order_delta
) {
    auto specs = build_edge_specs(o);
    std::vector<int> var_index(o.n, -1);
    size_t d = 0;
    for (size_t i = 0; i < o.n; ++i) {
        if (i == axis_line) continue;
        var_index[i] = (int)d++;
    }

    std::vector<std::vector<long double>> A;
    std::vector<long double> rhs;
    A.reserve(specs.size());
    rhs.reserve(specs.size());

    for (const auto& s : specs) {
        const long double c12 = b[s.b1] - b[s.b2];
        const long double c34 = b[s.b3] - b[s.b4];

        std::vector<long double> row(d, 0.0L);
        long double constant = 0.0L;

        auto add = [&](size_t line, long double coeff) {
            int vi = var_index[line];
            if (vi >= 0) {
                row[(size_t)vi] += coeff;
            } else {
                // Axis line has fixed dummy slope m_axis = 0.
                constant += coeff * 0.0L;
            }
        };

        add(s.m1, +c12);
        add(s.m2, -c12);
        add(s.m3, -c34);
        add(s.m4, +c34);

        long double scale = 0.0L;
        for (long double v : row) scale = std::max(scale, fabsl(v));
        long double r = -margin - constant;

        if (scale == 0.0L) {
            if (!(0.0L <= r)) {
                A.clear();
                rhs.clear();
                A.push_back(std::vector<long double>(d, 0.0L));
                rhs.push_back(-1.0L);
                return LinearSystem{A, rhs, var_index};
            }
            continue;
        }

        for (auto& v : row) v /= scale;
        r /= scale;
        A.push_back(std::move(row));
        rhs.push_back(r);
    }

    // Mandatory global wiring order at infinity:
    // either m_1 < m_2 < ... or the opposite orientation.
    std::vector<size_t> non_axis_lines;
    non_axis_lines.reserve(o.n > 0 ? o.n - 1 : 0);
    for (size_t i = 0; i < o.n; ++i) {
        if (i != axis_line) non_axis_lines.push_back(i);
    }
    for (size_t idx = 0; idx + 1 < non_axis_lines.size(); ++idx) {
        const size_t a = non_axis_lines[idx];
        const size_t c = non_axis_lines[idx + 1];
        std::vector<long double> row(d, 0.0L);
        if (m_order_increasing) {
            // m_c - m_a >= delta  <=>  m_a - m_c <= -delta
            row[(size_t)var_index[a]] = +1.0L;
            row[(size_t)var_index[c]] = -1.0L;
        } else {
            // m_a - m_c >= delta  <=>  -m_a + m_c <= -delta
            row[(size_t)var_index[a]] = -1.0L;
            row[(size_t)var_index[c]] = +1.0L;
        }
        A.push_back(std::move(row));
        rhs.push_back(-m_order_delta);
    }

    return LinearSystem{A, rhs, var_index};
}

struct SolveResult {
    bool feasible = false;
    long double max_violation = std::numeric_limits<long double>::infinity();
    long double t = std::numeric_limits<long double>::infinity();
    std::vector<long double> x;
};

static SolveResult solve_feasibility_lp(const LinearSystem& sys, long double tol) {
    SolveResult out;
    const size_t m = sys.A.size();
    const size_t d = sys.var_index.empty() ? 0 : (size_t)(*std::max_element(sys.var_index.begin(), sys.var_index.end()) + 1);

    if (m == 0) {
        out.feasible = true;
        out.max_violation = 0.0L;
        out.t = 0.0L;
        out.x.assign(d, 0.0L);
        return out;
    }

    const double inf = 1.0e30;
    const size_t t_col = d;
    const size_t num_vars = d + 1;

    std::vector<double> col_cost(num_vars, 0.0);
    std::vector<double> col_lower(num_vars, -inf);
    std::vector<double> col_upper(num_vars, +inf);
    col_cost[t_col] = 1.0;
    col_lower[t_col] = 0.0;

    std::vector<double> row_lower(m, -inf);
    std::vector<double> row_upper(m, 0.0);
    std::vector<HighsInt> a_start(m, 0);
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
        a_index.push_back((HighsInt)t_col);
        a_value.push_back(-1.0);
    }

    std::vector<double> col_value(num_vars, 0.0), col_dual(num_vars, 0.0);
    std::vector<double> row_value(m, 0.0), row_dual(m, 0.0);
    std::vector<HighsInt> col_basis_status(num_vars, 0), row_basis_status(m, 0);

    HighsInt model_status = kHighsModelStatusNotset;
    HighsInt run_status = Highs_lpCall(
        (HighsInt)num_vars,
        (HighsInt)m,
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

    out.x.assign(d, 0.0L);
    for (size_t j = 0; j < d; ++j) out.x[j] = (long double)col_value[j];
    out.t = (long double)col_value[t_col];

    long double maxv = -std::numeric_limits<long double>::infinity();
    for (size_t i = 0; i < m; ++i) {
        long double ax = 0.0L;
        for (size_t j = 0; j < d; ++j) ax += sys.A[i][j] * out.x[j];
        maxv = std::max(maxv, ax - sys.rhs[i]);
    }
    out.max_violation = maxv;

    out.feasible = (run_status == kHighsStatusOk &&
                    model_status == kHighsModelStatusOptimal &&
                    out.t <= tol &&
                    out.max_violation <= tol);
    return out;
}

static std::vector<long double> build_hyperbolic_b(
    const OMatrix& o,
    size_t axis_line,
    long double eps
) {
    if (o.n % 2 == 0) throw std::runtime_error("Only odd N is supported in hyperbolic-axis mode");
    if (!(eps > 0.0L) || !std::isfinite((double)eps)) throw std::runtime_error("--axis-eps must be finite and > 0");

    const size_t n = o.n;
    const size_t p = (n - 1) / 2;
    const long double pi = acosl(-1.0L);
    const long double delta = pi / (2.0L * (long double)p);

    if (eps >= delta) throw std::runtime_error("--axis-eps must be smaller than pi/(N-1)");

    const auto& row = o.intersections[axis_line];
    if (row.size() != n - 1) {
        throw std::runtime_error("Selected axis does not intersect all other lines (row size != N-1)");
    }

    std::vector<long double> thetas;
    thetas.reserve(n - 1);
    for (size_t k = p - 1; k >= 1; --k) {
        thetas.push_back(-(long double)k * delta);
        if (k == 1) break;
    }
    thetas.push_back(-eps);
    thetas.push_back(+eps);
    for (size_t k = 1; k <= p - 1; ++k) {
        thetas.push_back((long double)k * delta);
    }

    if (thetas.size() != n - 1) {
        throw std::runtime_error("Internal angle generation error");
    }

    std::vector<long double> b(n, 0.0L);
    b[axis_line] = 0.0L;
    for (size_t idx = 0; idx < row.size(); ++idx) {
        long double t = tanl(thetas[idx]);
        if (!std::isfinite((double)t)) {
            throw std::runtime_error("NaN/Inf generated in tan(theta)");
        }
        b[row[idx]] = t;
    }

    return b;
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
    bool active = false;
    std::string line;

    while (std::getline(in, line)) {
        size_t close_paren_pos = line.find(')');
        if (close_paren_pos != std::string::npos) {
            if (!current.empty()) {
                cases.push_back(current);
                current.clear();
            }
            active = true;
            std::string tail = line.substr(close_paren_pos + 1);
            auto nums = extract_nonnegative_ints(tail);
            current.insert(current.end(), nums.begin(), nums.end());
            continue;
        }

        if (active) {
            auto nums = extract_nonnegative_ints(line);
            current.insert(current.end(), nums.begin(), nums.end());
            continue;
        }

        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) continue;
        if (is_numbers_only_line(trimmed)) {
            auto nums = extract_nonnegative_ints(trimmed);
            if (!nums.empty()) {
                active = true;
                current.insert(current.end(), nums.begin(), nums.end());
            }
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

struct Options {
    std::string gens_str;
    bool have_gens = false;
    bool filter_mode = false;

    bool axis_set = false;
    size_t axis_k = 0;

    long double eps = 1e-5L;
    long double margin = 1.0L;
    bool try_reflect = false;
    std::string output_format = "full";
    bool metadata_only = false;

    long double tol = 1e-10L;
    long double m_order_delta = 1e-4L;
};

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\" [--hyperbolic-axis [K]] [--axis-eps E] [--margin M] [--try-reflect]\n"
        << "  " << argv0 << " --gens-file path.txt [--hyperbolic-axis [K]] [--axis-eps E] [--margin M] [--try-reflect]\n"
        << "  cat input.txt | " << argv0 << " [--hyperbolic-axis [K]] [--axis-eps E] [--margin M] [--try-reflect]\n"
        << "\n"
        << "Options:\n"
        << "  --hyperbolic-axis [K]   If K omitted, tries all axes 0..N-1\n"
        << "  --axis-eps E            Default 1e-5\n"
        << "  --margin M              Default 1.0\n"
        << "  --try-reflect           Also try reflected O-matrix\n"
        << "  --output-format compact|full|json\n"
        << "  --metadata-only         Filter mode: diagnostics only\n";
}

struct AttemptResult {
    bool skipped_unrotatable = false;
    bool valid = false;
    bool feasible = false;

    size_t axis_input = 0;
    size_t axis_used = 0;
    int rotation = 0;
    bool reflected = false;
    bool m_order_increasing = true;

    size_t n = 0;
    size_t gens_count = 0;
    long double margin = 0.0L;
    long double eps = 0.0L;
    long double output_rotation_phi = 0.0L;
    long double t = std::numeric_limits<long double>::infinity();
    long double max_violation = std::numeric_limits<long double>::infinity();
    long double worst_raw = std::numeric_limits<long double>::infinity();

    std::string error;
    std::vector<long double> m_orig;
    std::vector<long double> b_orig;
};

static std::vector<size_t> build_new_to_original_map(size_t n, bool reflected, int rotation) {
    std::vector<size_t> original_to_new(n, 0), new_to_original(n, 0);
    for (size_t orig = 0; orig < n; ++orig) {
        int after_reflect = reflected ? (int)(n - 1 - orig) : (int)orig;
        int nw = (after_reflect >= rotation) ? (after_reflect - rotation) : (after_reflect - rotation + (int)n);
        original_to_new[orig] = (size_t)nw;
    }
    for (size_t orig = 0; orig < n; ++orig) new_to_original[original_to_new[orig]] = orig;
    return new_to_original;
}

static bool better_attempt(const AttemptResult& a, const AttemptResult& b);

static void apply_small_output_rotation(AttemptResult& res) {
    if (res.n == 0 || res.axis_input >= res.n) return;
    if (res.m_orig.size() != res.n || res.b_orig.size() != res.n) return;

    long double max_abs_m = 0.0L;
    for (size_t i = 0; i < res.n; ++i) {
        if (i == res.axis_input) continue;
        max_abs_m = std::max(max_abs_m, fabsl(res.m_orig[i]));
    }

    long double phi = std::min(1e-6L, 1e-3L / std::max(1.0L, max_abs_m));
    const long double min_denom = 1e-8L;

    for (int iter = 0; iter < 40; ++iter) {
        long double c = cosl(phi);
        long double s = sinl(phi);
        long double best = std::numeric_limits<long double>::infinity();
        for (size_t i = 0; i < res.n; ++i) {
            if (i == res.axis_input) continue;
            long double d = c - res.m_orig[i] * s;
            best = std::min(best, fabsl(d));
        }
        if (best >= min_denom) break;
        phi *= 0.5L;
    }

    long double c = cosl(phi);
    long double s = sinl(phi);
    if (fabsl(s) < 1e-18L) {
        phi = 1e-12L;
        c = cosl(phi);
        s = sinl(phi);
    }

    std::vector<long double> m_new = res.m_orig;
    std::vector<long double> b_new = res.b_orig;

    for (size_t i = 0; i < res.n; ++i) {
        if (i == res.axis_input) continue;
        const long double m = res.m_orig[i];
        const long double b = res.b_orig[i];
        const long double d = c - m * s;
        if (!std::isfinite((double)d) || fabsl(d) < 1e-18L) continue;
        m_new[i] = (m * c + s) / d;
        b_new[i] = b / d;
    }

    // Axis x=0 rotated by phi: y = -cot(phi) * x.
    m_new[res.axis_input] = -c / s;
    b_new[res.axis_input] = 0.0L;

    res.m_orig = std::move(m_new);
    res.b_orig = std::move(b_new);
    res.output_rotation_phi = phi;
}

static AttemptResult run_attempt(
    const OMatrix& original,
    const std::vector<size_t>& gens,
    size_t axis_original,
    bool reflected,
    long double eps,
    long double margin,
    long double tol,
    long double m_order_delta
) {
    AttemptResult res;
    res.axis_input = axis_original;
    res.axis_used = axis_original;
    res.reflected = reflected;
    res.n = original.n;
    res.gens_count = gens.size();
    res.margin = margin;
    res.eps = eps;

    OMatrix base = reflected ? original.reflect() : original;

    const int rotation = reflected ? (int)(original.n - 1 - axis_original) : (int)axis_original;
    res.rotation = rotation;

    OMatrix rotated;
    if (!base.rotate(rotation, rotated)) {
        res.skipped_unrotatable = true;
        res.error = "SKIP_UNROTATABLE";
        return res;
    }

    try {
        const size_t axis_line = 0;
        std::vector<long double> b = build_hyperbolic_b(rotated, axis_line, eps);
        const bool m_order_increasing = true;
        LinearSystem sys = build_linear_system_for_m(
            rotated, b, axis_line, margin, m_order_increasing, m_order_delta
        );
        SolveResult sol = solve_feasibility_lp(sys, tol);

        std::vector<long double> m(rotated.n, 0.0L);
        m[axis_line] = 0.0L;
        for (size_t line = 0; line < rotated.n; ++line) {
            if (line == axis_line) continue;
            int vi = sys.var_index[line];
            if (vi >= 0 && (size_t)vi < sol.x.size()) m[line] = sol.x[(size_t)vi];
        }

        long double worst_raw = -std::numeric_limits<long double>::infinity();
        auto specs = build_edge_specs(rotated);
        for (const auto& s : specs) {
            long double c1 = m[s.m1] - m[s.m2];
            long double c2 = m[s.m3] - m[s.m4];
            long double raw = (b[s.b1] - b[s.b2]) * c1 - (b[s.b3] - b[s.b4]) * c2;
            worst_raw = std::max(worst_raw, raw);
        }

        res.t = sol.t;
        res.max_violation = sol.max_violation;
        res.worst_raw = worst_raw;
        res.feasible = (sol.feasible && worst_raw <= (-margin + tol));
        res.valid = true;
        res.m_order_increasing = true;

        auto new_to_original = build_new_to_original_map(original.n, reflected, rotation);
        res.m_orig.assign(original.n, 0.0L);
        res.b_orig.assign(original.n, 0.0L);
        for (size_t nw = 0; nw < original.n; ++nw) {
            size_t orig = new_to_original[nw];
            res.m_orig[orig] = m[nw];
            res.b_orig[orig] = b[nw];
        }
    } catch (const std::exception& e) {
        res.error = e.what();
        res.valid = false;
    }

    return res;
}

static bool better_attempt(const AttemptResult& a, const AttemptResult& b) {
    if (a.feasible != b.feasible) return a.feasible;
    if (a.t != b.t) return a.t < b.t;
    return a.max_violation < b.max_violation;
}

static std::string bool_to_json(bool v) { return v ? "true" : "false"; }

static void print_lines_csv_block(const std::vector<long double>& m, const std::vector<long double>& b) {
    std::cout << "#LINES_BEGIN\n";
    std::cout << "m,b\n";
    for (size_t i = 0; i < m.size(); ++i) {
        std::cout << m[i] << "," << b[i] << "\n";
    }
    std::cout << "#LINES_END\n";
}

static void print_result(const AttemptResult& r, const std::string& format, bool include_lines, size_t case_idx = 0) {
    const char* status = r.feasible ? "SAT" : "NO_SOLUTION";

    if (format == "json") {
        std::cout << "{";
        if (case_idx > 0) std::cout << "\"case\":" << case_idx << ",";
        std::cout << "\"status\":\"" << status << "\""
                  << ",\"n\":" << r.n
                  << ",\"gens_count\":" << r.gens_count
                  << ",\"axis_input\":" << r.axis_input
                  << ",\"axis_used\":" << r.axis_used
                  << ",\"rotation\":" << r.rotation
                  << ",\"reflected\":" << bool_to_json(r.reflected)
                  << ",\"m_order\":\"" << (r.m_order_increasing ? "inc" : "dec") << "\""
                  << ",\"margin\":" << std::setprecision(21) << r.margin
                  << ",\"eps\":" << r.eps
                  << ",\"output_rotation_phi\":" << r.output_rotation_phi
                  << ",\"t\":" << r.t
                  << ",\"max_violation\":" << r.max_violation
                  << ",\"worst_raw\":" << r.worst_raw;
        if (!r.error.empty()) std::cout << ",\"error\":\"" << r.error << "\"";
        if (include_lines && r.m_orig.size() == r.n && r.b_orig.size() == r.n) {
            std::cout << ",\"lines\":[";
            for (size_t i = 0; i < r.n; ++i) {
                if (i) std::cout << ",";
                std::cout << "[" << r.m_orig[i] << "," << r.b_orig[i] << "]";
            }
            std::cout << "]";
        }
        std::cout << "}\n";
        return;
    }

    if (format == "compact") {
        std::cout << status;
        if (case_idx > 0) std::cout << " #" << case_idx;
        std::cout << " n=" << r.n
                  << " gens=" << r.gens_count
                  << " axis_input=" << r.axis_input
                  << " axis_used=" << r.axis_used
                  << " rotation=" << r.rotation
                  << " reflected=" << (r.reflected ? 1 : 0)
                  << " m_order=" << (r.m_order_increasing ? "inc" : "dec")
                  << " margin=" << std::setprecision(21) << r.margin
                  << " eps=" << r.eps
                  << " output_rotation_phi=" << r.output_rotation_phi
                  << " t=" << r.t
                  << " max_violation=" << r.max_violation
                  << " worst_raw=" << r.worst_raw;
        if (!r.error.empty()) std::cout << " error=" << r.error;
        std::cout << "\n";
        return;
    }

    std::cout << status << "\n";
    std::cout << "n=" << r.n
              << " gens_count=" << r.gens_count
              << " axis_input=" << r.axis_input
              << " axis_used=" << r.axis_used
              << " rotation=" << r.rotation
              << " reflected=" << (r.reflected ? 1 : 0)
              << " m_order=" << (r.m_order_increasing ? "inc" : "dec")
              << " margin=" << std::setprecision(21) << r.margin
              << " eps=" << r.eps
              << " output_rotation_phi=" << r.output_rotation_phi
              << " t=" << r.t
              << " max_violation=" << r.max_violation
              << " worst_raw=" << r.worst_raw;
    if (!r.error.empty()) std::cout << " error=" << r.error;
    std::cout << "\n";

    if (include_lines && r.m_orig.size() == r.n && r.b_orig.size() == r.n) {
        print_lines_csv_block(r.m_orig, r.b_orig);
    }
}

static AttemptResult solve_case(const std::vector<size_t>& gens, const Options& opt) {
    OMatrix o = make_omatrix(gens);
    if (o.n % 2 == 0) throw std::runtime_error("Only odd N supported (got even N)");

    std::vector<AttemptResult> attempts;
    if (opt.axis_set) {
        if (opt.axis_k >= o.n) throw std::runtime_error("--hyperbolic-axis out of range");
        attempts.push_back(run_attempt(o, gens, opt.axis_k, false, opt.eps, opt.margin, opt.tol, opt.m_order_delta));
        if (opt.try_reflect) {
            attempts.push_back(run_attempt(o, gens, opt.axis_k, true, opt.eps, opt.margin, opt.tol, opt.m_order_delta));
        }
    } else {
        for (size_t k = 0; k < o.n; ++k) {
            attempts.push_back(run_attempt(o, gens, k, false, opt.eps, opt.margin, opt.tol, opt.m_order_delta));
            if (opt.try_reflect) {
                attempts.push_back(run_attempt(o, gens, k, true, opt.eps, opt.margin, opt.tol, opt.m_order_delta));
            }
        }
    }

    bool have_candidate = false;
    AttemptResult best;
    for (const auto& a : attempts) {
        if (a.skipped_unrotatable) continue;
        if (!have_candidate || better_attempt(a, best)) {
            best = a;
            have_candidate = true;
        }
    }

    if (!have_candidate) {
        AttemptResult no;
        no.n = o.n;
        no.gens_count = gens.size();
        no.axis_input = opt.axis_set ? opt.axis_k : 0;
        no.axis_used = no.axis_input;
        no.rotation = 0;
        no.reflected = false;
        no.margin = opt.margin;
        no.eps = opt.eps;
        no.feasible = false;
        no.valid = false;
        no.error = "SKIP_UNROTATABLE";
        return no;
    }

    if (best.feasible) {
        apply_small_output_rotation(best);
    }
    return best;
}

int main(int argc, char** argv) {
    try {
        Options opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            }
            if (arg == "--gens") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --gens");
                opt.gens_str = argv[++i];
                opt.have_gens = true;
                continue;
            }
            if (arg == "--gens-file") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --gens-file");
                if (!read_file(argv[++i], opt.gens_str)) throw std::runtime_error("Failed to read gens file");
                opt.have_gens = true;
                continue;
            }
            if (arg == "--hyperbolic-axis") {
                opt.axis_set = true;
                if (i + 1 < argc) {
                    std::string nxt = argv[i + 1];
                    if (!nxt.empty() && nxt[0] != '-') {
                        size_t pos = 0;
                        unsigned long long v = std::stoull(nxt, &pos);
                        if (pos != nxt.size()) throw std::runtime_error("Invalid value for --hyperbolic-axis");
                        opt.axis_k = (size_t)v;
                        ++i;
                    } else {
                        opt.axis_set = false;
                    }
                } else {
                    opt.axis_set = false;
                }
                continue;
            }
            if (arg == "--axis-eps") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --axis-eps");
                opt.eps = std::stold(argv[++i]);
                continue;
            }
            if (arg == "--margin") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --margin");
                opt.margin = std::stold(argv[++i]);
                continue;
            }
            if (arg == "--try-reflect") {
                opt.try_reflect = true;
                continue;
            }
            if (arg == "--output-format") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --output-format");
                opt.output_format = argv[++i];
                if (opt.output_format != "compact" && opt.output_format != "full" && opt.output_format != "json") {
                    throw std::runtime_error("--output-format must be compact|full|json");
                }
                continue;
            }
            if (arg == "--metadata-only") {
                opt.metadata_only = true;
                continue;
            }
            if (!arg.empty() && arg[0] != '-') {
                std::ostringstream ss;
                ss << arg;
                for (int j = i + 1; j < argc; ++j) ss << ' ' << argv[j];
                opt.gens_str = ss.str();
                opt.have_gens = true;
                break;
            }
            throw std::runtime_error("Unknown option: " + arg);
        }

        if (!opt.have_gens && !isatty(STDIN_FILENO)) opt.filter_mode = true;

        if (!(opt.margin > 0.0L) || !std::isfinite((double)opt.margin)) {
            throw std::runtime_error("--margin must be finite and > 0");
        }

        if (opt.filter_mode) {
            auto cases = parse_filter_cases_from_stream(std::cin);
            if (cases.empty()) throw std::runtime_error("No cases parsed from stdin");

            size_t idx = 0;
            for (const auto& gens : cases) {
                if (gens.empty()) continue;
                ++idx;
                try {
                    auto result = solve_case(gens, opt);
                    bool include_lines = (!opt.metadata_only && opt.output_format == "full");
                    print_result(result, opt.output_format, include_lines, idx);
                } catch (const std::exception& e) {
                    AttemptResult err;
                    err.feasible = false;
                    err.error = e.what();
                    err.gens_count = gens.size();
                    if (!gens.empty()) {
                        err.n = *std::max_element(gens.begin(), gens.end()) + 2;
                    }
                    err.margin = opt.margin;
                    err.eps = opt.eps;
                    print_result(err, opt.output_format, false, idx);
                }
            }
            return 0;
        }

        if (!opt.have_gens) throw std::runtime_error("No generators provided");
        auto gens = parse_gens_numbers(opt.gens_str);
        if (gens.empty()) throw std::runtime_error("No valid generator numbers parsed");

        auto result = solve_case(gens, opt);
        const bool include_lines = !(opt.metadata_only && opt.filter_mode);
        print_result(result, opt.output_format, include_lines && opt.output_format == "full", 0);
        return result.feasible ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
