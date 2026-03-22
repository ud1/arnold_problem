#include <algorithm>
#include <cmath>
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

#include "common/input_parsing.h"
#include "common/omatrix.h"
#include "common/solver_backend.h"
#include "solver/custom_phase1_solver.h"
#include "solver/highs_phase1_solver.h"

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
    BackendKind backend_used = BackendKind::Highs;
};

static SolveResult solve_feasibility_lp_highs(const LinearSystem& sys, long double tol) {
    SolveResult out;
    out.backend_used = BackendKind::Highs;
    highs_phase1::Result highs = highs_phase1::solve(sys.A, sys.rhs, tol);
    out.x = std::move(highs.x);
    out.max_violation = highs.max_violation;
    out.t = highs.t;
    out.feasible = highs.feasible;
    return out;
}

static SolveResult solve_feasibility_lp_custom(const LinearSystem& sys, long double tol) {
    SolveResult out;
    out.backend_used = BackendKind::Custom;
    custom_phase1::Result custom = custom_phase1::solve(sys.A, sys.rhs, tol);
    out.feasible = custom.feasible;
    out.max_violation = custom.max_violation;
    out.t = custom.t;
    out.x = std::move(custom.x);
    return out;
}

static SolveResult solve_feasibility_lp(const LinearSystem& sys, long double tol, BackendKind backend) {
    return solve_with_backend_fallback<SolveResult>(
        backend,
        [&]() { return solve_feasibility_lp_highs(sys, tol); },
        [&]() { return solve_feasibility_lp_custom(sys, tol); }
    );
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

struct Options {
    std::string gens_str;
    bool have_gens = false;
    bool filter_mode = false;

    size_t axis_k = 0;

    long double eps = 1e-5L;
    long double margin = 1e-3L;
    bool try_reflect = false;
    bool euclidean_rotations = false;
    bool projective_rotations = false;
    bool print_sat_gens = false;
    bool print_sat_lines = false;
    std::string output_format = "full";
    bool metadata_only = false;

    long double tol = 1e-10L;
    long double m_order_delta = 1e-4L;
    BackendKind solver_backend = BackendKind::Highs;
};

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\" [--axis K|-a K] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p]\n"
        << "  " << argv0 << " --gens-file path.txt [--axis K|-a K] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p]\n"
        << "  cat input.txt | " << argv0 << " [--axis K|-a K] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p]\n"
        << "\n"
        << "Options:\n"
        << "  --axis K, -a K          Axis index K (default 0)\n"
        << "  --axis-eps E            Default 1e-5\n"
        << "  --margin M              Default 0.001\n"
        << "  --try-reflect           Also try reflected O-matrix\n"
        << "  --euclidean-rotations, -e  Try all Euclidean O-matrix rotations\n"
        << "  --projective-rotations, -p  Try identity plus all projective O-matrix rotations\n"
        << "  --print-sat-gens        Filter mode: print generators for SAT cases\n"
        << "  --print-sat-lines       Filter mode: print line equations for SAT cases\n"
        << "  --output-format compact|full|json\n"
        << "  --metadata-only         Filter mode: diagnostics only\n"
        << "  --solver highs|custom|both (default highs)\n";
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
    int euclidean_rotation = 0;
    int projective_rotation = -1;
    int tried_projective_rotations = 1;
    int tried_euclidean_rotations = 1;
    BackendKind solver_backend = BackendKind::Highs;

    std::string error;
    std::vector<long double> m_orig;
    std::vector<long double> b_orig;
    std::vector<size_t> omatrix_gens;
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

static bool build_output_lines_swapped_xy(
    const AttemptResult& res,
    std::vector<long double>& m_out,
    std::vector<long double>& b_out
) {
    if (res.n == 0 || res.axis_input >= res.n) return false;
    if (res.m_orig.size() != res.n || res.b_orig.size() != res.n) return false;

    m_out.assign(res.n, 0.0L);
    b_out.assign(res.n, 0.0L);

    for (size_t i = 0; i < res.n; ++i) {
        if (i == res.axis_input) {
            // Axis x=0 becomes y=0 after x <-> y.
            m_out[i] = 0.0L;
            b_out[i] = 0.0L;
            continue;
        }

        const long double m = res.m_orig[i];
        const long double b = res.b_orig[i];
        if (!std::isfinite((double)m) || fabsl(m) < 1e-18L) {
            return false;
        }

        m_out[i] = 1.0L / m;
        b_out[i] = -b / m;
    }

    return true;
}

static AttemptResult run_attempt(
    const OMatrix& original,
    const std::vector<size_t>& gens,
    size_t axis_original,
    bool reflected,
    long double eps,
    long double margin,
    long double tol,
    long double m_order_delta,
    BackendKind solver_backend
) {
    AttemptResult res;
    res.axis_input = axis_original;
    res.axis_used = axis_original;
    res.reflected = reflected;
    res.n = original.n;
    res.gens_count = gens.size();
    res.margin = margin;
    res.eps = eps;
    res.omatrix_gens = omatrix_to_generators(original);

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
        auto specs = build_edge_specs(rotated);
        struct OrderCandidate {
            bool m_order_increasing = true;
            bool feasible = false;
            long double t = std::numeric_limits<long double>::infinity();
            long double max_violation = std::numeric_limits<long double>::infinity();
            long double worst_raw = std::numeric_limits<long double>::infinity();
            BackendKind solver_backend = BackendKind::Highs;
            std::vector<long double> m;
        };

        auto solve_with_order = [&](bool m_order_increasing) {
            OrderCandidate cand;
            cand.m_order_increasing = m_order_increasing;

            LinearSystem sys = build_linear_system_for_m(
                rotated, b, axis_line, margin, m_order_increasing, m_order_delta
            );
            SolveResult sol = solve_feasibility_lp(sys, tol, solver_backend);

            std::vector<long double> m(rotated.n, 0.0L);
            m[axis_line] = 0.0L;
            for (size_t line = 0; line < rotated.n; ++line) {
                if (line == axis_line) continue;
                int vi = sys.var_index[line];
                if (vi >= 0 && (size_t)vi < sol.x.size()) m[line] = sol.x[(size_t)vi];
            }

            long double worst_raw = -std::numeric_limits<long double>::infinity();
            for (const auto& s : specs) {
                long double c1 = m[s.m1] - m[s.m2];
                long double c2 = m[s.m3] - m[s.m4];
                long double raw = (b[s.b1] - b[s.b2]) * c1 - (b[s.b3] - b[s.b4]) * c2;
                worst_raw = std::max(worst_raw, raw);
            }

            cand.t = sol.t;
            cand.max_violation = sol.max_violation;
            cand.worst_raw = worst_raw;
            cand.feasible = (sol.feasible && worst_raw <= (-margin + tol));
            cand.solver_backend = sol.backend_used;
            cand.m = std::move(m);
            return cand;
        };

        OrderCandidate inc = solve_with_order(true);
        OrderCandidate dec = solve_with_order(false);
        auto better_candidate = [](const OrderCandidate& a, const OrderCandidate& b) {
            if (a.feasible != b.feasible) return a.feasible;
            if (a.t != b.t) return a.t < b.t;
            return a.max_violation < b.max_violation;
        };
        const OrderCandidate& best = better_candidate(inc, dec) ? inc : dec;

        res.t = best.t;
        res.max_violation = best.max_violation;
        res.worst_raw = best.worst_raw;
        res.feasible = best.feasible;
        res.valid = true;
        res.m_order_increasing = best.m_order_increasing;
        res.solver_backend = best.solver_backend;

        auto new_to_original = build_new_to_original_map(original.n, reflected, rotation);
        res.m_orig.assign(original.n, 0.0L);
        res.b_orig.assign(original.n, 0.0L);
        for (size_t nw = 0; nw < original.n; ++nw) {
            size_t orig = new_to_original[nw];
            res.m_orig[orig] = best.m[nw];
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

static void print_generators_line(const std::vector<size_t>& gens, const std::string& prefix = "GENS") {
    std::cout << prefix << " [";
    for (size_t i = 0; i < gens.size(); ++i) {
        if (i) std::cout << " ";
        std::cout << gens[i];
    }
    std::cout << "]\n";
}

static void print_result(const AttemptResult& r, const std::string& format, bool include_lines, size_t case_idx = 0) {
    const char* status = r.feasible ? "SAT" : "NO_SOLUTION";
    std::vector<long double> output_m;
    std::vector<long double> output_b;
    const bool have_output_lines = r.feasible && include_lines && build_output_lines_swapped_xy(r, output_m, output_b);

    if (format == "json") {
        std::cout << "{";
        if (case_idx > 0) std::cout << "\"case\":" << case_idx << ",";
        std::cout << "\"status\":\"" << status << "\""
                  << ",\"n\":" << r.n
                  << ",\"gens_count\":" << r.gens_count
                  << ",\"projective_rotation\":" << r.projective_rotation
                  << ",\"tried_projective_rotations\":" << r.tried_projective_rotations
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
                  << ",\"worst_raw\":" << r.worst_raw
                  << ",\"solver\":\"" << backend_kind_name(r.solver_backend) << "\"";
        if (!r.error.empty()) std::cout << ",\"error\":\"" << r.error << "\"";
        if (have_output_lines) {
            std::cout << ",\"lines\":[";
            for (size_t i = 0; i < r.n; ++i) {
                if (i) std::cout << ",";
                std::cout << "[" << output_m[i] << "," << output_b[i] << "]";
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
                  << " projective_rotation=" << r.projective_rotation
                  << " tried_projective_rotations=" << r.tried_projective_rotations
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
                  << " worst_raw=" << r.worst_raw
                  << " solver=" << backend_kind_name(r.solver_backend);
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
              << " worst_raw=" << r.worst_raw
              << " solver=" << backend_kind_name(r.solver_backend);
    if (!r.error.empty()) std::cout << " error=" << r.error;
    std::cout << "\n";
    std::cout << "projective_rotation=" << r.projective_rotation
              << " tried_projective_rotations=" << r.tried_projective_rotations
              << "\n";

    if (have_output_lines) {
        print_lines_csv_block(output_m, output_b);
    }
}

static AttemptResult solve_case(const std::vector<size_t>& gens, const Options& opt) {
    OMatrix base = make_omatrix(gens);
    if (base.n % 2 == 0) throw std::runtime_error("Only odd N supported (got even N)");
    if (opt.axis_k >= base.n) throw std::runtime_error("--axis out of range");

    auto solve_for_omatrix = [&](const OMatrix& o, int projective_rotation) {
        std::vector<AttemptResult> attempts;
        attempts.push_back(run_attempt(o, gens, opt.axis_k, false, opt.eps, opt.margin, opt.tol, opt.m_order_delta, opt.solver_backend));
        if (opt.try_reflect) {
            attempts.push_back(run_attempt(o, gens, opt.axis_k, true, opt.eps, opt.margin, opt.tol, opt.m_order_delta, opt.solver_backend));
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
            no.axis_input = opt.axis_k;
            no.axis_used = no.axis_input;
            no.rotation = 0;
            no.reflected = false;
            no.margin = opt.margin;
            no.eps = opt.eps;
            no.solver_backend = opt.solver_backend;
            no.projective_rotation = projective_rotation;
            no.feasible = false;
            no.valid = false;
            no.error = "SKIP_UNROTATABLE";
            return no;
        }

        best.projective_rotation = projective_rotation;
        return best;
    };

    auto solve_for_omatrix_with_euclidean = [&](const OMatrix& o, int projective_rotation) {
        const int max_rotation = opt.euclidean_rotations ? 2 * (int)o.n : 1;
        int tried_rotations = 0;
        AttemptResult best;
        bool have_best = false;
        for (int rot = 0; rot < max_rotation; ++rot) {
        OMatrix current;
        if (!o.rotate(rot, current)) continue;
        ++tried_rotations;
        AttemptResult cur = solve_for_omatrix(current, projective_rotation);
        cur.euclidean_rotation = rot;
        cur.omatrix_gens = omatrix_to_generators(current);
        if (!have_best || better_attempt(cur, best)) {
            best = std::move(cur);
            have_best = true;
        }
        }

        if (!have_best) {
            throw std::runtime_error("No valid Euclidean rotations available");
        }
        best.tried_euclidean_rotations = tried_rotations;
        return best;
    };

    if (!opt.projective_rotations) {
        AttemptResult single = solve_for_omatrix_with_euclidean(base, -1);
        single.tried_projective_rotations = 1;
        return single;
    }

    AttemptResult best;
    bool have_best = false;
    int tried_projective_rotations = 0;
    int total_tried_euclidean = 0;
    for (int p = -1; p < (int)base.n; ++p) {
        OMatrix current;
        if (p < 0) {
            current = base;
        } else if (!base.projective_rotate((size_t)p, current)) {
            continue;
        }
        ++tried_projective_rotations;

        AttemptResult cur = solve_for_omatrix_with_euclidean(current, p);
        total_tried_euclidean += cur.tried_euclidean_rotations;
        if (!have_best || better_attempt(cur, best)) {
            best = std::move(cur);
            have_best = true;
        }
    }

    if (!have_best) {
        throw std::runtime_error("No valid projective rotations available");
    }
    best.tried_projective_rotations = tried_projective_rotations;
    best.tried_euclidean_rotations = total_tried_euclidean;
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
            if (arg == "--axis" || arg == "-a") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --axis");
                std::string nxt = argv[++i];
                if (nxt.empty() || nxt[0] == '-') {
                    throw std::runtime_error("Invalid value for --axis");
                }
                size_t pos = 0;
                unsigned long long v = std::stoull(nxt, &pos);
                if (pos != nxt.size()) throw std::runtime_error("Invalid value for --axis");
                opt.axis_k = (size_t)v;
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
            if (arg == "--euclidean-rotations" || arg == "-e") {
                opt.euclidean_rotations = true;
                continue;
            }
            if (arg == "--projective-rotations" || arg == "-p") {
                opt.projective_rotations = true;
                opt.euclidean_rotations = true;
                continue;
            }
            if (arg == "--print-sat-gens") {
                opt.print_sat_gens = true;
                continue;
            }
            if (arg == "--print-sat-lines") {
                opt.print_sat_lines = true;
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
            if (arg == "--solver") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --solver");
                opt.solver_backend = parse_backend_kind(argv[++i]);
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
                    std::cout << (result.feasible ? "SAT" : "NO_SOLUTION")
                              << " #" << idx
                              << " n=" << result.n
                              << " gens=" << result.gens_count
                              << " eucl_rot=" << result.euclidean_rotation << "/" << result.tried_euclidean_rotations
                              << " proj_rot=" << result.projective_rotation << "/" << result.tried_projective_rotations
                              << " margin=" << std::setprecision(21) << result.margin
                              << " t=" << result.t
                              << " worst_raw=" << result.worst_raw
                              << " solver=" << backend_kind_name(result.solver_backend);
                    if (!result.error.empty()) std::cout << " error=" << result.error;
                    std::cout << "\n";
                    if (result.feasible && opt.print_sat_lines &&
                        result.m_orig.size() == result.n && result.b_orig.size() == result.n) {
                        std::vector<long double> output_m;
                        std::vector<long double> output_b;
                        if (build_output_lines_swapped_xy(result, output_m, output_b)) {
                            std::cout << "#LINES_BEGIN #" << idx << "\n";
                            std::cout << "m,b\n";
                            for (size_t i = 0; i < result.n; ++i) {
                                std::cout << output_m[i] << "," << output_b[i] << "\n";
                            }
                            std::cout << "#LINES_END #" << idx << "\n";
                        }
                    }
                    if (result.feasible && opt.print_sat_gens) {
                        print_generators_line(result.omatrix_gens, "GENS #" + std::to_string(idx));
                    }
                    std::cout.flush();
                } catch (const std::exception& e) {
                    std::cout << "ERROR"
                              << " #" << idx
                              << " message=" << e.what()
                              << "\n";
                    std::cout.flush();
                } catch (...) {
                    std::cout << "ERROR"
                              << " #" << idx
                              << " message=unknown exception"
                              << "\n";
                    std::cout.flush();
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
        if (result.feasible && opt.print_sat_gens) {
            print_generators_line(result.omatrix_gens);
        }
        return result.feasible ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}
