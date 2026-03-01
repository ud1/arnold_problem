#include <algorithm>
#include <array>
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
#include <vector>

#include "interfaces/highs_c_api.h"

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    size_t n = 0;
};

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

    return OMatrix{intersections, n};
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
    const HighsInt num_col = (HighsInt)d;
    const HighsInt num_row = (HighsInt)m;

    std::vector<double> col_cost(d, 0.0);
    std::vector<double> col_lower(d, -inf);
    std::vector<double> col_upper(d, +inf);
    std::vector<double> row_lower(m, -inf);
    std::vector<double> row_upper(m, 0.0);

    std::vector<HighsInt> a_start(m, 0);
    std::vector<HighsInt> a_index;
    std::vector<double> a_value;
    a_index.reserve(m * (d > 0 ? d : 1));
    a_value.reserve(m * (d > 0 ? d : 1));

    for (size_t i = 0; i < m; ++i) {
        a_start[i] = (HighsInt)a_index.size();
        row_upper[i] = (double)sys.rhs[i];
        for (size_t j = 0; j < d; ++j) {
            long double v = sys.A[i][j];
            if (v == 0.0L) continue;
            a_index.push_back((HighsInt)j);
            a_value.push_back((double)v);
        }
    }

    std::vector<double> col_value(d, 0.0), col_dual(d, 0.0);
    std::vector<double> row_value(m, 0.0), row_dual(m, 0.0);
    std::vector<HighsInt> col_basis_status(d, 0), row_basis_status(m, 0);

    HighsInt model_status = kHighsModelStatusNotset;
    HighsInt run_status = Highs_lpCall(
        num_col,
        num_row,
        (HighsInt)a_index.size(),
        kHighsMatrixFormatRowwise,
        kHighsObjSenseMinimize,
        0.0,
        col_cost.data(),
        d ? col_lower.data() : nullptr,
        d ? col_upper.data() : nullptr,
        m ? row_lower.data() : nullptr,
        m ? row_upper.data() : nullptr,
        m ? a_start.data() : nullptr,
        a_index.empty() ? nullptr : a_index.data(),
        a_value.empty() ? nullptr : a_value.data(),
        d ? col_value.data() : nullptr,
        d ? col_dual.data() : nullptr,
        m ? row_value.data() : nullptr,
        m ? row_dual.data() : nullptr,
        d ? col_basis_status.data() : nullptr,
        m ? row_basis_status.data() : nullptr,
        &model_status
    );

    std::vector<long double> x(d, 0.0L);
    for (size_t j = 0; j < d; ++j) x[j] = (long double)col_value[j];

    long double max_violation = -std::numeric_limits<long double>::infinity();
    for (size_t i = 0; i < m; ++i) {
        long double ax = 0.0L;
        for (size_t j = 0; j < d; ++j) ax += sys.A[i][j] * x[j];
        max_violation = std::max(max_violation, ax - sys.rhs[i]);
    }

    out.x = std::move(x);
    out.max_violation = max_violation;
    out.t = out.max_violation;
    out.newton_steps = 0;
    out.feasible = (run_status == kHighsStatusOk &&
                    model_status != kHighsModelStatusInfeasible &&
                    model_status != kHighsModelStatusUnboundedOrInfeasible &&
                    out.max_violation <= 1e-10L);
    return out;
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

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\" [--axis-pair [I,J]] [--axis-eps E]\n"
        << "  " << argv0 << " --gens-file path.txt [--axis-pair [I,J]] [--axis-eps E]\n"
        << "  cat input.txt | " << argv0 << " [--axis-pair [I,J]] [--axis-eps E]\n"
        << "\n"
        << "Only axis-pair mode is supported.\n"
        << "  --axis-pair without I,J: anchor boundary lines 0 and n-1\n"
        << "  --axis-pair I,J: anchor adjacent lines I and J\n";
}

int main(int argc, char** argv) {
    try {
        std::string gens_str;
        bool have_gens = false;
        bool filter_mode = false;

        AxisModeConfig axis;
        bool axis_pair_seen = false;

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

        const long double strict_margin = 1.0L;

        if (filter_mode) {
            auto cases = parse_filter_cases_from_stream(std::cin);
            if (cases.empty()) throw std::runtime_error("No cases parsed from stdin");

            for (size_t ci = 0; ci < cases.size(); ++ci) {
                const auto& gens = cases[ci];
                if (gens.empty()) continue;

                OMatrix o = make_omatrix(gens);
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

                const bool sat = (worst_raw < -1e-10L);
                std::cout << (sat ? "SAT" : "NO_SOLUTION")
                          << " #" << (ci + 1)
                          << " n=" << o.n
                          << " gens=" << gens.size()
                          << " margin_target=" << (double)strict_margin
                          << " worst_raw=" << std::setprecision(18) << (double)worst_raw
                          << " t=" << (double)sr.t
                          << "\n";
            }
            return 0;
        }

        if (!have_gens) throw std::runtime_error("No generators provided");
        auto gens = parse_gens_numbers(gens_str);
        if (gens.empty()) throw std::runtime_error("No valid generator numbers parsed");

        OMatrix o = make_omatrix(gens);
        BuildResult br = build_fixed_slopes(o.n, axis);

        LinearSystem sys = build_linear_system(o, br.m, br.axis_left, br.axis_right, strict_margin);
        SolveResult sr = solve_feasibility_lp(sys);

        std::vector<long double> b(o.n, 0.0L);
        for (size_t line = 0; line < o.n; ++line) {
            if (line == br.axis_left || line == br.axis_right) {
                b[line] = 0.0L;
                continue;
            }
            int vi = sys.var_index[line];
            if (vi >= 0 && (size_t)vi < sr.x.size()) b[line] = sr.x[(size_t)vi];
        }

        // Recheck max raw directly with produced b.
        long double worst_raw = -std::numeric_limits<long double>::infinity();
        auto specs = build_edge_specs(o);
        for (const auto& s : specs) {
            long double c1 = br.m[s.m1] - br.m[s.m2];
            long double c2 = br.m[s.m3] - br.m[s.m4];
            long double raw = (b[s.b1] - b[s.b2]) * c1 - (b[s.b3] - b[s.b4]) * c2;
            worst_raw = std::max(worst_raw, raw);
        }

        const bool sat = (worst_raw < -1e-10L);

        std::cout << std::setprecision(18);
        if (sat) {
            std::cout << "SAT\n";
        } else {
            std::cout << "NO_SOLUTION\n";
        }

        std::cout << "n=" << o.n
                  << " gens=" << gens.size()
                  << " axis_left=" << br.axis_left
                  << " axis_right=" << br.axis_right
                  << " axis_eps=" << (double)axis.eps
                  << " margin_target=" << (double)strict_margin
                  << " newton=" << sr.newton_steps
                  << " t=" << (double)sr.t
                  << " max_violation_lp=" << (double)sr.max_violation
                  << " worst_raw=" << (double)worst_raw
                  << "\n";

        std::cout << "LINES [";
        for (size_t i = 0; i < o.n; ++i) {
            if (i) std::cout << ", ";
            std::cout << "(" << (double)br.m[i] << "," << (double)b[i] << ")";
        }
        std::cout << "]\n";

        return sat ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
