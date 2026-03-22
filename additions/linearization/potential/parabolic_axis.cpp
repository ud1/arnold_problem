#include <algorithm>
#include <array>
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
#include <vector>

#include "common/input_parsing.h"
#include "common/omatrix.h"
#include "common/rotation_search.h"
#include "common/solver_backend.h"
#include "solver/custom_phase1_solver.h"
#include "solver/highs_phase1_solver.h"

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
    long double strict_margin
) {
    const auto specs = build_edge_specs(o);
    std::vector<int> var_index(o.n, -1);
    size_t d = o.n;
    for (size_t i = 0; i < o.n; ++i) var_index[i] = (int)i;

    std::vector<std::vector<long double>> A;
    std::vector<long double> rhs;
    A.reserve(specs.size());
    rhs.reserve(specs.size());

    for (const auto& s : specs) {
        long double c1 = m[s.m1] - m[s.m2];
        long double c2 = m[s.m3] - m[s.m4];

        std::vector<long double> row(d, 0.0L);

        auto add_coeff = [&](size_t line, long double coeff) {
            int vi = var_index[line];
            if (vi >= 0) row[(size_t)vi] += coeff;
        };

        add_coeff(s.b1, +c1);
        add_coeff(s.b2, -c1);
        add_coeff(s.b3, -c2);
        add_coeff(s.b4, +c2);

        long double row_scale = 0.0L;
        for (long double v : row) row_scale = std::max(row_scale, fabsl(v));
        long double r = -strict_margin;

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
    BackendKind backend_used = BackendKind::Highs;
};

static SolveResult solve_feasibility_lp_highs(const LinearSystem& sys, long double tol) {
    SolveResult out;
    out.backend_used = BackendKind::Highs;
    highs_phase1::Result highs = highs_phase1::solve(sys.A, sys.rhs, tol);
    out.x = std::move(highs.x);
    out.max_violation = highs.max_violation;
    out.t = highs.t;
    out.newton_steps = 0;
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
    out.newton_steps = custom.iterations;
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

struct SolveParams {
    long double tol = 1e-10L;
    BackendKind backend = BackendKind::Highs;
};

struct AttemptResult {
    bool sat = false;
    bool reflected = false;
    int projective_rotation = -1;
    int rotation = 0;
    size_t n = 0;
    size_t gens_count = 0;
    size_t axis_left = 0;
    size_t axis_right = 0;
    long double axis_eps = 0.0L;
    long double margin = 0.0L;
    long double t = std::numeric_limits<long double>::infinity();
    long double max_violation_lp = std::numeric_limits<long double>::infinity();
    long double worst_raw = std::numeric_limits<long double>::infinity();
    std::vector<long double> m;
    std::vector<long double> b;
    std::vector<size_t> omatrix_gens;
    BackendKind solver_backend = BackendKind::Highs;
};

static AttemptResult solve_for_omatrix(
    const OMatrix& o,
    size_t gens_count,
    const AxisModeConfig& axis,
    long double strict_margin,
    bool reflected,
    int rotation,
    const SolveParams& solve_params
) {
    if (!validate_omatrix(o, false)) {
        throw std::runtime_error("Invalid O-matrix before solve");
    }

    BuildResult br = build_fixed_slopes(o.n, axis);
    LinearSystem sys = build_linear_system(o, br.m, strict_margin);
    SolveResult sr = solve_feasibility_lp(sys, solve_params.tol, solve_params.backend);

    std::vector<long double> b(o.n, 0.0L);
    for (size_t line = 0; line < o.n; ++line) {
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
    out.reflected = reflected;
    out.rotation = rotation;
    out.n = o.n;
    out.gens_count = gens_count;
    out.axis_left = br.axis_left;
    out.axis_right = br.axis_right;
    out.axis_eps = axis.eps;
    out.margin = strict_margin;
    out.t = sr.t;
    out.max_violation_lp = sr.max_violation;
    out.worst_raw = worst_raw;
    out.m = std::move(br.m);
    out.b = std::move(b);
    out.omatrix_gens = omatrix_to_generators(o);
    out.solver_backend = sr.backend_used;
    return out;
}

static bool parabolic_better(const AttemptResult& a, const AttemptResult& b) {
    if (a.sat && !b.sat) return true;
    if (!a.sat && b.sat) return false;
    return a.worst_raw < b.worst_raw;
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

static void print_filter_result(
    const AttemptResult& res,
    const std::vector<size_t>& gens,
    size_t case_index,
    int tried_projective_rotations,
    int tried_plane_rotations,
    bool print_gens,
    bool print_sat_gens,
    bool print_sat_lines
) {
    std::cout << (res.sat ? "SAT" : "NO_SOLUTION")
              << " #" << case_index
              << " n=" << res.n
              << " gens=" << gens.size()
              << " reflected=" << (res.reflected ? 1 : 0)
              << " eucl_rot=" << res.rotation << "/" << tried_plane_rotations
              << " proj_rot=" << res.projective_rotation << "/" << tried_projective_rotations
              << " margin=" << std::setprecision(21) << (double)res.margin
              << " worst_raw=" << std::setprecision(18) << (double)res.worst_raw
              << " t=" << (double)res.t
              << " solver=" << backend_kind_name(res.solver_backend)
              << "\n";
    if (res.sat && print_sat_lines) {
        std::cout << "#LINES_BEGIN #" << case_index << "\n";
        std::cout << "m,b\n";
        for (size_t i = 0; i < res.m.size() && i < res.b.size(); ++i) {
            std::cout << (double)res.m[i] << "," << (double)res.b[i] << "\n";
        }
        std::cout << "#LINES_END #" << case_index << "\n";
    }
    if (res.sat && print_sat_gens) {
        print_generators_line(res.omatrix_gens, "GENS #" + std::to_string(case_index));
    }
    if (print_gens) {
        print_generators_line(res.omatrix_gens, "GENS #" + std::to_string(case_index));
    }
    std::cout.flush();
}

static AttemptResult solve_case_full(
    const OMatrix& base,
    size_t gens_count,
    const AxisModeConfig& axis,
    long double strict_margin,
    const RotationSearchConfig& rot_cfg,
    RotationStats& rot_stats,
    const SolveParams& solve_params
) {
    return rotation_search<AttemptResult>(
        base, rot_cfg, rot_stats,
        [&](const OMatrix& o, bool reflected, int eucl_rot, int proj_rot) {
            AttemptResult r = solve_for_omatrix(o, gens_count, axis, strict_margin, reflected, eucl_rot, solve_params);
            r.projective_rotation = proj_rot;
            return r;
        },
        parabolic_better
    );
}

static void process_filter_case(
    const std::vector<size_t>& gens,
    size_t case_index,
    const AxisModeConfig& axis,
    long double strict_margin,
    const RotationSearchConfig& rot_cfg,
    bool print_gens,
    bool print_sat_gens,
    bool print_sat_lines,
    const SolveParams& solve_params
) {
    try {
        OMatrix o = make_omatrix(gens);
        RotationStats rot_stats;
        AttemptResult res = solve_case_full(o, gens.size(), axis, strict_margin, rot_cfg, rot_stats, solve_params);
        print_filter_result(res, gens, case_index,
            rot_stats.tried_projective, rot_stats.tried_euclidean,
            print_gens, print_sat_gens, print_sat_lines);
    } catch (const std::exception& e) {
        std::cout << "ERROR"
                  << " #" << case_index
                  << " message=" << e.what()
                  << "\n";
        std::cout.flush();
    } catch (...) {
        std::cout << "ERROR"
                  << " #" << case_index
                  << " message=unknown exception"
                  << "\n";
        std::cout.flush();
    }
}

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --gens \"0 1 0 2 ...\" [--axis [I,J]|-a [I,J]] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p] [--print-gens]\n"
        << "  " << argv0 << " --gens-file path.txt [--axis [I,J]|-a [I,J]] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p] [--print-gens]\n"
        << "  cat input.txt | " << argv0 << " [--axis [I,J]|-a [I,J]] [--axis-eps E] [--margin M] [--try-reflect] [--euclidean-rotations|-e] [--projective-rotations|-p] [--print-gens]\n"
        << "\n"
        << "Only axis mode is supported.\n"
        << "  --axis without I,J (or -a): anchor boundary lines 0 and n-1\n"
        << "  --axis I,J (or -a I,J): anchor adjacent lines I and J\n"
        << "  --margin M: strict margin target (default 1.0)\n"
        << "  --try-reflect, -r: also try reflected O-matrix\n"
        << "  --euclidean-rotations, -e: iterate all valid O-matrix plane rotations\n"
        << "  --projective-rotations, -p: iterate all projective rotations; implies -e\n"
        << "  --print-gens: print generators for the O-matrix used to produce LINES\n"
        << "  --print-sat-gens: in filter mode, print generators only for SAT cases\n"
        << "  --print-sat-lines: in filter mode, print line equations only for SAT cases\n"
        << "  --solver highs|custom|both: LP backend (default highs)\n";
}

int main(int argc, char** argv) {
    try {
        std::string gens_str;
        bool have_gens = false;
        bool filter_mode = false;

        AxisModeConfig axis;
        bool axis_seen = false;
        RotationSearchConfig rot_cfg;
        bool print_gens = false;
        bool print_sat_gens = false;
        bool print_sat_lines = false;
        SolveParams solve_params;
        long double strict_margin = 1.0L;
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
            if (arg == "--axis" || arg == "-a") {
                axis_seen = true;
                axis.boundary_pair = true;
                if (i + 1 < argc) {
                    std::string next = argv[i + 1];
                    if (!next.empty() && next[0] != '-' && next.find(',') != std::string::npos) {
                        size_t l = 0, r = 0;
                        if (!parse_axis_pair_spec(next, l, r)) {
                            throw std::runtime_error("Invalid --axis format, expected I,J");
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
            if (arg == "--margin") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --margin");
                strict_margin = std::stold(argv[++i]);
                continue;
            }
            if (arg == "--try-reflect" || arg == "-r") {
                rot_cfg.try_reflect = true;
                continue;
            }
            if (arg == "--euclidean-rotations" || arg == "-e") {
                rot_cfg.euclidean_rotations = true;
                continue;
            }
            if (arg == "--projective-rotations" || arg == "-p") {
                rot_cfg.projective_rotations = true;
                rot_cfg.euclidean_rotations = true;
                continue;
            }
            if (arg == "--print-gens") {
                print_gens = true;
                continue;
            }
            if (arg == "--print-sat-gens") {
                print_sat_gens = true;
                continue;
            }
            if (arg == "--print-sat-lines") {
                print_sat_lines = true;
                continue;
            }
            if (arg == "--solver") {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for --solver");
                solve_params.backend = parse_backend_kind(argv[++i]);
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
        if (!axis_seen) {
            axis_seen = true;
            axis.boundary_pair = true;
        }
        if (rot_cfg.euclidean_rotations && !axis.boundary_pair) {
            throw std::runtime_error("--euclidean-rotations supports only boundary axis mode");
        }
        if (rot_cfg.projective_rotations && !axis.boundary_pair) {
            throw std::runtime_error("--projective-rotations supports only boundary axis mode");
        }
        if (!(strict_margin > 0.0L) || !std::isfinite((double)strict_margin)) {
            throw std::runtime_error("--margin must be finite and > 0");
        }

        if (filter_mode) {
            FilterCaseStreamState state;
            std::vector<size_t> gens;
            size_t case_index = 0;
            while (read_next_filter_case(std::cin, state, gens)) {
                if (gens.empty()) continue;
                ++case_index;
                process_filter_case(
                    gens, case_index, axis, strict_margin, rot_cfg,
                    print_gens, print_sat_gens, print_sat_lines, solve_params
                );
            }
            if (case_index == 0) throw std::runtime_error("No cases parsed from stdin");
            return 0;
        }

        if (!have_gens) throw std::runtime_error("No generators provided");
        auto gens = parse_gens_numbers(gens_str);
        if (gens.empty()) throw std::runtime_error("No valid generator numbers parsed");

        OMatrix o = make_omatrix(gens);
        RotationStats rot_stats;
        AttemptResult res = solve_case_full(o, gens.size(), axis, strict_margin, rot_cfg, rot_stats, solve_params);

        std::cout << std::setprecision(18);
        if (res.sat) {
            std::cout << "SAT\n";
        } else {
            std::cout << "NO_SOLUTION\n";
        }

        std::cout << "n=" << res.n
                  << " gens=" << res.gens_count
                  << " reflected=" << (res.reflected ? 1 : 0)
                  << " rotation=" << res.rotation
                  << " tried_plane_rotations=" << rot_stats.tried_euclidean
                  << " axis_left=" << res.axis_left
                  << " axis_right=" << res.axis_right
                  << " axis_eps=" << (double)res.axis_eps
                  << " margin=" << std::setprecision(21) << (double)res.margin
                  << " newton=0"
                  << " t=" << (double)res.t
                  << " max_violation_lp=" << (double)res.max_violation_lp
                  << " worst_raw=" << (double)res.worst_raw
                  << " solver=" << backend_kind_name(res.solver_backend)
                  << "\n";
        std::cout << "projective_rotation=" << res.projective_rotation
                  << " tried_projective_rotations=" << rot_stats.tried_projective
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
