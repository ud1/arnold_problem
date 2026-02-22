#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <limits>
#include <iomanip>

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
        << "  " << argv0 << " [--max-steps N] [--log-every N] [--step X]\n"
        << "       [--min-edge X] [--min-angle X]\n"
        << "       [--log-format compact|full]\n"
        << "\n"
        << "Notes:\n"
        << "  Default is 'no limit' on iterations.\n"
        << "  --max-steps N sets a limit; --max-steps 0 means 'no limit'.\n"
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

    struct SatStats {
        bool strict_ok;
        bool margin_ok;
        size_t strict_violations;
        size_t margin_violations;
        double worst_raw;        // max raw (should be < 0 for strict)
        double worst_shifted;    // max(raw + MIN_*) (should be < 0 for margin_ok)
    };

    Terms(OMatrix &o) {
        size_t n = o.n;

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

        auto m = [](size_t i){return i;};
        auto b = [n](size_t i){return i + n;};


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

    double value() const
    {
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
};

int main(int argc, char** argv) {
    std::string gens_str =
        "1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 3 5 7 9 11 13 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 3 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 5 7 9 11 10 9 8 7 6 5 7 9 11 13 12 11 13 15 14 13 12 11 10 9 8 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 5 7 6 7 9 8 7 9 11 13 15 14 13 12 11 13 15 17 16 15 14 13 12 11 10 9 8 9 11 10 12 1 3 5 7 9 11 13 15 17";

    size_t max_steps_limit = 0;
    size_t LOG_EVERY = 10000;
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
        if (arg == "--log-every") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --log-every\n";
                return 2;
            }
            LOG_EVERY = (size_t)std::stoull(argv[++argi]);
            continue;
        }
        if (arg == "--log-format") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --log-format\n";
                return 2;
            }
            std::string v = argv[++argi];
            if (v == "compact") log_format = LogFormat::Compact;
            else if (v == "full") log_format = LogFormat::Full;
            else {
                std::cerr << "Unknown --log-format: " << v << "\n";
                return 2;
            }
            continue;
        }
        if (arg == "--step") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --step\n";
                return 2;
            }
            step = std::stod(argv[++argi]);
            continue;
        }
        if (arg == "--min-edge") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --min-edge\n";
                return 2;
            }
            g_min_edge = std::stod(argv[++argi]);
            continue;
        }
        if (arg == "--min-angle") {
            if (argi + 1 >= argc) {
                std::cerr << "Missing value for --min-angle\n";
                return 2;
            }
            g_min_angle = std::stod(argv[++argi]);
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
    bool prev_satisfied = false;
    size_t iter_done = 0;
    const double STEP_DEC = 0.5;
    const double STEP_INC = 1.1;
    const double ARMIJO_C = 0.5;
    const double STEP_MIN = 1e-14;
    const double STEP_MAX = 1.0;
    terms.calc_grad();
    for (size_t i = 0; unlimited_steps || i < max_steps_limit; ++i)
    {
        terms.prev_grad = terms.grad;
        iter_done = i;
        double val = terms.value();
        if (val < best_result) best_result = val;
        double grad2 = terms.calc_grad(); // also updates terms.grad buffer
        double grad = std::sqrt(grad2);
        if (LOG_EVERY != 0 && i % LOG_EVERY == 0) {
            auto st = terms.sat_stats();
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
                          << " vS=" << std::setw(3) << std::dec << st.strict_violations
                          << " S=" << (st.strict_ok ? 1 : 0)
                          << std::defaultfloat
                          << std::endl;
            }

            if (st.strict_ok && prev_satisfied) {
                iter_done = i;
                break;
            }
            prev_satisfied = st.strict_ok;
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

    {
        // Final snapshot: everything needed for reading the result without scanning logs.
        // calc_grad() updates internal grad buffer; compute it before printing summary.
        double final_val = terms.value();
        double final_grad = std::sqrt(terms.calc_grad());
        auto st = terms.sat_stats();
        if (st.strict_ok) {
            std::cerr
                << "SAT_FOUND"
                << " strict=1"
                << " margin=" << (st.margin_ok ? 1 : 0)
                << " worst_raw=" << st.worst_raw
                << " worst_shifted=" << st.worst_shifted
                << "\n";
        } else {
            std::cerr
                << "NO_SAT_FOUND"
                << " strict=0"
                << " margin=" << (st.margin_ok ? 1 : 0)
                << " worst_raw=" << st.worst_raw
                << " worst_shifted=" << st.worst_shifted
                << "\n";
        }

        std::cerr
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
            << " best_V=" << best_result
            << "\n";
    }

    terms.print();

    return 0;
}
