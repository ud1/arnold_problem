#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <charconv>
#include <cstring>
#include <cmath>

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

const double MIN_EDGE = 1.0;
const double MIN_EDGE_FL = 1.0;
const double MIN_ANGLE = 0.01;

// y_ij = (x0_j - x0_i)/(m_i - m_j)
// y_ij > y_ik
// (x0_j - x0_i)/(m_i - m_j) > (x0_k - x0_i)/(m_i - m_k)
// (x0_k - x0_i)*(m_i - m_j) - (x0_j - x0_i) * (m_i - m_k) < 0
struct EdgeTerm {
    size_t i, j, k;
    double sign;
};


struct TermsConfig {
    size_t max_steps = 1000000;
    size_t enhance_steps = 300000;
    double min_decay = 0.0001;
    bool enhance = false;
    bool verbose = false;
    double enhance_angle_func_scale = 1000.0;
};

struct Params {
    std::vector<double> m;
    std::vector<double> x;
};

struct LossAndGrad {
    double loss;
    double pure_loss;
    std::vector<double> grad_m;
    std::vector<double> grad_x;
    bool solved;
    size_t ens, ans;
    double min_len;
    double max_len;

    void init(const Params &p) {
        grad_m.resize(p.m.size());
        grad_x.resize(p.x.size());
    }

    void reset() {
        loss = 0.0;
        pure_loss = 0.0;
        for (auto &g : grad_m)
            g = 0.0;
        for (auto &g : grad_x)
            g = 0.0;
        solved = true;
        ens = 0;
        ans = 0;
        min_len = 1e100;
        max_len = 0.0;
    }

    double g2() {
        double res = 0.0;
        for (auto &g : grad_m)
            res += g * g;
        for (auto &g : grad_x)
            res += g * g;

        return res;
    }
};

struct Results {
    LossAndGrad lg;
    size_t iters = 0;
};

// Line equation
// x = x0 + m*y
// Line intersection
// y_ij = (x0_j - x0_i)/(m_i - m_j)
// x_ij = (m_i*x0_j - m_j*x0_i)/(m_i - m_j)
// angle condition
// i < j => m_i > m_j
struct Problem {
    std::vector<EdgeTerm> edgeTerms;
    std::vector<std::pair<size_t, size_t>> extreme_points;
    double angle_func_scale = 1000.0;
    double angle_func_scale2;
};

void retransform_m(Params &p) {
    double max_m = 1.0/std::tan(M_PI / (double) p.m.size());
    double min_v = p.m[p.m.size() - 1];
    double max_v = p.m[0];
    double sum = 0.0;
    size_t sum_start = p.m.size() / 3;
    size_t sum_end = p.m.size() - sum_start;
    for (size_t i = 0; i < p.m.size(); ++i) {
        p.m[i] = -max_m + 2.0*max_m*(p.m[i] - min_v) / (max_v - min_v);

        if (i >= sum_start && i < sum_end)
            sum += p.m[i];
    }

    sum /= (double) (sum_end - sum_start);
    for (auto &m : p.m) {
        m -= sum;
    }
}

void retransform_x(Params &p) {
    double min_v = *std::min_element(p.x.begin(), p.x.end());
    double max_v = *std::max_element(p.x.begin(), p.x.end());
    double sum = 0.0;
    for (auto &x : p.x) {
        x = -20.0 + 40.0*(x - min_v)/(max_v - min_v);
        sum += x;
    }
    sum /= (double) p.x.size();
    for (auto &x : p.x) {
        x -= sum;
    }
}

typedef void (*compute_loss_and_grad_func)(LossAndGrad &res, const Params &params, const Problem &p);
void compute_loss_and_grad_initial(LossAndGrad &res, const Params &params, const Problem &p) {
    res.reset();

    size_t n = params.m.size();
    for (size_t i = 0; i < n-1; ++i) {
        double dm = params.m[i + 1] - params.m[i];
        if (dm > 0.0) {
            res.solved = false;
            res.ans++;
            res.pure_loss += dm * dm * p.angle_func_scale;
        }
        dm += MIN_ANGLE;
        if (dm > 0.0) {
            res.loss += dm*dm * p.angle_func_scale;
            double g = 2.0*dm * p.angle_func_scale;
            res.grad_m[i] -= g;
            res.grad_m[i + 1] += g;
        }
    }

    for (auto &t : p.edgeTerms) {
        double dm = ((params.x[t.j] - params.x[t.i]) * (params.m[t.i] - params.m[t.k]) - (params.x[t.k] - params.x[t.i]) * (params.m[t.i] - params.m[t.j]))*t.sign;
        if (dm > 0.0) {
            res.solved = false;
            res.ens++;
            res.pure_loss += dm * dm;
        }
        dm += MIN_EDGE;
        if (dm > 0.0) {
            res.loss += dm*dm;

            double val = dm * 2.0;
            double b1 = (params.x[t.j] - params.x[t.i]) * t.sign;
            double b2 = (params.x[t.k] - params.x[t.i]) * t.sign;

            res.grad_m[t.i] += val * (b1 - b2);
            res.grad_m[t.j] += val * b2;
            res.grad_m[t.k] -= val * b1;

            double a = (params.m[t.i] - params.m[t.k]) * t.sign;
            double b = (params.m[t.i] - params.m[t.j]) * t.sign;

            res.grad_x[t.i] += val * (b - a);
            res.grad_x[t.j] += val * a;
            res.grad_x[t.k] -= val * b;
        }
    }
}

void compute_loss_and_grad_enhance(LossAndGrad &res, const Params &params, const Problem &p) {
    res.reset();

    size_t n = params.m.size();
    for (size_t i = 0; i < n; ++i) {
        if (i + 1 < n && params.m[i] < params.m[i + 1]) {
            res.solved = false;
            res.ans++;
        }
        if (i == 0) {
            size_t last_i = params.m.size() - 1;
            double angle = M_PI - std::atan2(params.m[0], 1.0) + std::atan2(params.m[last_i], 1.0);
            res.loss += p.angle_func_scale2 / (angle * angle);
            res.grad_m[0] += 2.0*p.angle_func_scale2/(angle*angle*angle*(1.0 + params.m[0]*params.m[0]));
            res.grad_m[last_i] -= 2.0*p.angle_func_scale2/(angle*angle*angle*(1.0 + params.m[last_i]*params.m[last_i]));
        }
        if (i < params.m.size() - 1) {
            double angle = std::atan2(params.m[i], 1.0) - std::atan2(params.m[i + 1], 1.0);
            res.loss += p.angle_func_scale2 / (angle * angle);
            double g = -2.0*p.angle_func_scale2/(angle*angle*angle*(1.0 + params.m[i]*params.m[i]));
            res.grad_m[i] += g;
            g = 2.0*p.angle_func_scale2/(angle*angle*angle*(1.0 + params.m[i + 1]*params.m[i + 1]));
            res.grad_m[i + 1] += g;
        }
    }

    for (auto &t : p.edgeTerms) {
        double dm = ((params.x[t.j] - params.x[t.i]) * (params.m[t.i] - params.m[t.k]) - (params.x[t.k] - params.x[t.i]) * (params.m[t.i] - params.m[t.j]))*t.sign;
        if (dm > 0.0) {
            res.solved = false;
            res.ens++;
        }
        size_t i = t.i;
        size_t j = t.j;
        size_t k = t.k;

        auto &ep = p.extreme_points[i];
        size_t j1 = ep.first;
        size_t k1 = ep.second;

        // ----------------------
        // numerator: scaleCoef for (i, j1, k1)
        // ----------------------
        double dm1_ij = params.m[i] - params.m[j1];
        double dm1_ik = params.m[i] - params.m[k1];

        double x1_ij = (params.x[j1] * params.m[i] - params.x[i] * params.m[j1]) / dm1_ij;
        double y1_ij = (params.x[j1] - params.x[i]) / dm1_ij;

        double x1_ik = (params.x[k1] * params.m[i] - params.x[i] * params.m[k1]) / dm1_ik;
        double y1_ik = (params.x[k1] - params.x[i]) / dm1_ik;

        double dx1 = x1_ik - x1_ij;
        double dy1 = y1_ik - y1_ij;

        double scaleCoef = dx1 * dx1 + dy1 * dy1;

        // ----------------------
        // denominator: len2 for (i, j, k)
        // ----------------------
        double dm_ij = params.m[i] - params.m[j];
        double dm_ik = params.m[i] - params.m[k];

        double x_ij = (params.x[j] * params.m[i] - params.x[i] * params.m[j]) / dm_ij;
        double y_ij = (params.x[j] - params.x[i]) / dm_ij;

        double x_ik = (params.x[k] * params.m[i] - params.x[i] * params.m[k]) / dm_ik;
        double y_ik = (params.x[k] - params.x[i]) / dm_ik;

        double dx = x_ik - x_ij;
        double dy = y_ik - y_ij;

        double len2 = dx * dx + dy * dy;

        {
            double l2 = len2 / scaleCoef;
            if (res.min_len > l2)
                res.min_len = l2;
            if (res.max_len < l2)
                res.max_len = l2;
        }

        res.loss += scaleCoef / len2;

        double inv_len2 = 1.0 / len2;
        double inv_len4 = 1.0 / (len2 * len2);

        double coeff_num =  2.0 * inv_len2;
        double coeff_den = -2.0 * scaleCoef * inv_len4;

        // ----------------------
        // gradients wrt m, denominator (i,j,k)
        // ----------------------
        double d_dx_dmi = params.m[k] * (params.x[i] - params.x[k]) / (dm_ik * dm_ik)
                        - params.m[j] * (params.x[i] - params.x[j]) / (dm_ij * dm_ij);
        double d_dy_dmi = (params.x[i] - params.x[k]) / (dm_ik * dm_ik)
                        - (params.x[i] - params.x[j]) / (dm_ij * dm_ij);

        double d_dx_dmj = params.m[i] * (params.x[i] - params.x[j]) / (dm_ij * dm_ij);
        double d_dy_dmj = (params.x[i] - params.x[j]) / (dm_ij * dm_ij);

        double d_dx_dmk = params.m[i] * (params.x[k] - params.x[i]) / (dm_ik * dm_ik);
        double d_dy_dmk = (params.x[k] - params.x[i]) / (dm_ik * dm_ik);

        // ----------------------
        // gradients wrt m, numerator (i,j1,k1)
        // ----------------------
        double d_dx1_dmi = params.m[k1] * (params.x[i] - params.x[k1]) / (dm1_ik * dm1_ik)
                         - params.m[j1] * (params.x[i] - params.x[j1]) / (dm1_ij * dm1_ij);
        double d_dy1_dmi = (params.x[i] - params.x[k1]) / (dm1_ik * dm1_ik)
                         - (params.x[i] - params.x[j1]) / (dm1_ij * dm1_ij);

        double d_dx1_dmj1 = params.m[i] * (params.x[i] - params.x[j1]) / (dm1_ij * dm1_ij);
        double d_dy1_dmj1 = (params.x[i] - params.x[j1]) / (dm1_ij * dm1_ij);

        double d_dx1_dmk1 = params.m[i] * (params.x[k1] - params.x[i]) / (dm1_ik * dm1_ik);
        double d_dy1_dmk1 = (params.x[k1] - params.x[i]) / (dm1_ik * dm1_ik);

        // ----------------------
        // gradients wrt x, denominator (i,j,k)
        // ----------------------
        double d_dx_dxi = -params.m[k] / dm_ik + params.m[j] / dm_ij;
        double d_dy_dxi = -1.0 / dm_ik + 1.0 / dm_ij;

        double d_dx_dxj = -params.m[i] / dm_ij;
        double d_dy_dxj = -1.0 / dm_ij;

        double d_dx_dxk = params.m[i] / dm_ik;
        double d_dy_dxk = 1.0 / dm_ik;

        // ----------------------
        // gradients wrt x, numerator (i,j1,k1)
        // ----------------------
        double d_dx1_dxi = -params.m[k1] / dm1_ik + params.m[j1] / dm1_ij;
        double d_dy1_dxi = -1.0 / dm1_ik + 1.0 / dm1_ij;

        double d_dx1_dxj1 = -params.m[i] / dm1_ij;
        double d_dy1_dxj1 = -1.0 / dm1_ij;

        double d_dx1_dxk1 = params.m[i] / dm1_ik;
        double d_dy1_dxk1 = 1.0 / dm1_ik;

        // ----------------------
        // accumulate gradients
        // ----------------------
        res.grad_m[i]  += coeff_num * (dx1 * d_dx1_dmi  + dy1 * d_dy1_dmi)
                        + coeff_den * (dx  * d_dx_dmi   + dy  * d_dy_dmi);
        res.grad_m[j]  += coeff_den * (dx * d_dx_dmj + dy * d_dy_dmj);
        res.grad_m[k]  += coeff_den * (dx * d_dx_dmk + dy * d_dy_dmk);
        res.grad_m[j1] += coeff_num * (dx1 * d_dx1_dmj1 + dy1 * d_dy1_dmj1);
        res.grad_m[k1] += coeff_num * (dx1 * d_dx1_dmk1 + dy1 * d_dy1_dmk1);

        res.grad_x[i]  += coeff_num * (dx1 * d_dx1_dxi  + dy1 * d_dy1_dxi)
                        + coeff_den * (dx  * d_dx_dxi   + dy  * d_dy_dxi);
        res.grad_x[j]  += coeff_den * (dx * d_dx_dxj + dy * d_dy_dxj);
        res.grad_x[k]  += coeff_den * (dx * d_dx_dxk + dy * d_dy_dxk);
        res.grad_x[j1] += coeff_num * (dx1 * d_dx1_dxj1 + dy1 * d_dy1_dxj1);
        res.grad_x[k1] += coeff_num * (dx1 * d_dx1_dxk1 + dy1 * d_dy1_dxk1);
    }
}

void init(const OMatrix &o, Params &params, Problem &p) {
    size_t n = o.n;

    params.m.clear();
    for (size_t i = 0; i < n; ++i) {
        params.m.push_back((double) n * 0.5 - (double) i);
    }

    params.x.resize(n, 0);
    p.edgeTerms.clear();
    p.extreme_points.clear();
    for (size_t i = 0; i < o.intersections.size(); ++i) {
        auto &line = o.intersections[i];

        if (line.size() >= 2) {
            for (size_t ind = 0; ind < line.size() - 1; ++ind) {
                size_t j = line[ind];
                size_t k = line[ind + 1];
                double sign = (i > j) == (i > k) ? +1.0 : -1.0;
                p.edgeTerms.emplace_back(i, j, k, sign);
            }
        }

        p.extreme_points.emplace_back(line[0], line[line.size() - 1]);
    }
}

// double eval_loss_only(const Params &params, const Problem &p, compute_loss_and_grad_func compute_loss_and_grad) {
//     LossAndGrad tmp;
//     tmp.init(params);
//     compute_loss_and_grad(tmp, params, p);
//     return tmp.loss;
// }
//
// void check_grad(const Params &params, const Problem &p, compute_loss_and_grad_func compute_loss_and_grad) {
//     const double eps = 1e-7;
//
//     LossAndGrad lg;
//     lg.init(params);
//     compute_loss_and_grad(lg, params, p);
//
//     for (size_t i = 0; i < params.m.size(); ++i) {
//         Params p1 = params;
//         Params p2 = params;
//
//         p1.m[i] += eps;
//         p2.m[i] -= eps;
//
//         double f1 = eval_loss_only(p1, p, compute_loss_and_grad);
//         double f2 = eval_loss_only(p2, p, compute_loss_and_grad);
//
//         double num = (f1 - f2) / (2.0 * eps);
//         double ana = lg.grad_m[i];
//
//         std::cout << "mi=" << i
//                   << " analytic=" << ana
//                   << " numeric=" << num
//                   << " diff=" << std::abs(ana - num)
//                   << "\n";
//     }
//
//     for (size_t i = 0; i < params.x.size(); ++i) {
//         Params p1 = params;
//         Params p2 = params;
//
//         p1.x[i] += eps;
//         p2.x[i] -= eps;
//
//         double f1 = eval_loss_only(p1, p, compute_loss_and_grad);
//         double f2 = eval_loss_only(p2, p, compute_loss_and_grad);
//
//         double num = (f1 - f2) / (2.0 * eps);
//         double ana = lg.grad_x[i];
//
//         std::cout << "xi=" << i
//                   << " analytic=" << ana
//                   << " numeric=" << num
//                   << " diff=" << std::abs(ana - num)
//                   << "\n";
//     }
// }

Results solve(size_t iters, const Problem &p, Params &params, compute_loss_and_grad_func compute_loss_and_grad, bool enhance) {
    LossAndGrad lg1, lg2;
    lg1.init(params);
    lg2.init(params);

    LossAndGrad *prev_lg = &lg1;
    LossAndGrad *new_lg  = &lg2;

    Results results;
    Params sp;

    double step = 0.01;

    compute_loss_and_grad(*prev_lg, params, p);

    for (; results.iters < iters; ++results.iters) {
        double g2 = prev_lg->g2();

        if (g2 < 1e-30) {
            results.lg = *prev_lg;
            return results;
        }

        sp = params;

        bool accepted = false;
        for (int bt = 0; bt < 1000; ++bt) {
            for (size_t i = 0; i < params.m.size(); ++i) {
                params.m[i] = sp.m[i] - step * prev_lg->grad_m[i];
            }
            for (size_t i = 0; i < params.x.size(); ++i) {
                params.x[i] = sp.x[i] - step * prev_lg->grad_x[i];
            }

            compute_loss_and_grad(*new_lg, params, p);

            if (!enhance || new_lg->solved) {
                if (new_lg->loss - prev_lg->loss <= -0.5 * step * g2) {
                    accepted = true;
                    break;
                }
            }

            step *= 0.5;
            if (step < 1e-100) {
                results.lg = *prev_lg;
                // check_grad(params, p, compute_loss_and_grad);
                return results;
            }
        }

        if (!accepted) {
            results.lg = *prev_lg;
            return results;
        }

        // Barzilai-Borwein: alternating long/short
        double sts = 0.0;
        double sty = 0.0;
        double yty = 0.0;

        for (size_t i = 0; i < params.m.size(); ++i) {
            double sm = params.m[i] - sp.m[i];
            double ym = new_lg->grad_m[i] - prev_lg->grad_m[i];

            sts += sm * sm;
            sty += sm * ym;
            yty += ym * ym;
        }

        for (size_t i = 0; i < params.x.size(); ++i) {
            double sx = params.x[i] - sp.x[i];
            double yx = new_lg->grad_x[i] - prev_lg->grad_x[i];

            sts += sx * sx;
            sty += sx * yx;
            yty += yx * yx;
        }

        if (results.iters % 2 == 1) {
            // long BB: alpha = (s^T s) / (s^T y)
            if (sty > 1e-30) {
                step = sts / sty;
            } else {
                step *= 1.1;
            }
        } else {
            // short BB: alpha = (s^T y) / (y^T y)
            if (yty > 1e-30 && sty > 1e-30) {
                step = sty / yty;
            } else {
                step *= 1.1;
            }
        }

        std::swap(prev_lg, new_lg);
    }

    results.lg = *prev_lg;
    return results;
}

struct Stretch {
    OMatrix o;
    TermsConfig config;
    Problem problem;
    Params params;
    Results res;

    Stretch(const std::vector<size_t> &gens, const TermsConfig &terms_config) {
        o = makeOMatrix(gens);
        config = terms_config;
        init(o, params, problem);
        problem.angle_func_scale2 = terms_config.enhance_angle_func_scale;
    }

    void stretch() {
        double prev_pure_loss = -1.0;

        const size_t ITERS = 100000;
        for (;;) {
            Results results = solve(ITERS, problem, params, compute_loss_and_grad_initial, false);
            res.iters += results.iters;
            res.lg = results.lg;
            if (results.lg.solved) {
                return;
            }

            if (results.iters < ITERS) {
                return;
            }

            if (results.iters >= config.max_steps) {
                return;
            }

            if (config.verbose)
                std::cout << results.lg.pure_loss << " ENS " << results.lg.ens << " ANS " << results.lg.ans << std::endl;

            if (prev_pure_loss > 0.0) {
                double d = (prev_pure_loss - results.lg.pure_loss) / prev_pure_loss;
                if (d < config.min_decay)
                    return;
            }

            prev_pure_loss = results.lg.pure_loss;
        }
    }

    void enhance() {
        retransform_m(params);
        retransform_x(params);

        const size_t ITERS = 100000;
        const size_t MAX_I = (config.enhance_steps + ITERS - 1) / ITERS;
        for (size_t i = 0; i < MAX_I; ++i) {
            Results results = solve(ITERS, problem, params, compute_loss_and_grad_enhance, true);
            if (config.verbose)
                std::cout << "E " << results.lg.loss << std::endl;
            res.iters += results.iters;
            res.lg = results.lg;
            if (results.iters < ITERS) {
                break;
            }
        }

        retransform_x(params);
    }

    void print() {
        size_t n = params.m.size();
        for (size_t i = 0; i < n; ++i) {
            double x = params.x[i];
            double m = params.m[i];
            m = -1.0/m;
            double b = -x*m;
            std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << m << ","
                << std::setprecision(std::numeric_limits<double>::max_digits10) << b << std::endl;
        }
    }
};


int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "Example usage:" << std::endl;
        std::cout << "linGrad --gens \"1 2 3 ...\"" << std::endl;
        std::cout << "linGrad --file file_name --min_steps 100000 --max_steps 1000000 --decay 0.001 -e -v" << std::endl;
        std::cout << "--decay means a minimum relative change of the pure potential per 100k steps required to continue linearization" << std::endl;
        return -1;
    }
    std::string gens;
    TermsConfig terms_config;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--gens") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--gens)" << std::endl;
                return -1;
            }
            gens = argv[i + 1];
        }
        else if (std::string(argv[i]) == "--max_steps") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--max_steps)" << std::endl;
                return -1;
            }
            terms_config.max_steps = std::stoul(argv[i + 1]);
        }
        else if (std::string(argv[i]) == "--enhance_steps") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--enhance_steps)" << std::endl;
                return -1;
            }
            terms_config.enhance_steps = std::stoul(argv[i + 1]);
        }
        else if (std::string(argv[i]) == "--decay") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--decay)" << std::endl;
                return -1;
            }
            size_t len = strlen(argv[i + 1]);
            auto res = std::from_chars(argv[i + 1], argv[i + 1] + len, terms_config.min_decay, std::chars_format::general);
            if (res.ec != std::errc{} || res.ptr != (argv[i + 1] + len))
                throw std::invalid_argument("bad decay value");
        }
        else if (std::string(argv[i]) == "-e") {
            terms_config.enhance = true;
        }
        else if (std::string(argv[i]) == "-v") {
            terms_config.verbose = true;
        }
    }

    {
        Stretch s{gens_str_to_vec(gens), terms_config};
        s.stretch();

        std::cout << "RES:" << s.res.lg.solved << std::endl;
        std::cout << "PVAL:" << s.res.lg.pure_loss << std::endl;
        std::cout << "STEPS:" << s.res.iters << std::endl;
        std::cout << "N:" << s.o.n << std::endl;

        if (s.res.lg.solved) {
            if (terms_config.enhance) {
                s.enhance();
                std::cout << "VAL: " << s.res.lg.loss << " VAL_RATIO: " << std::sqrt(s.res.lg.max_len / s.res.lg.min_len) << " " << std::sqrt(s.res.lg.max_len) << std::endl;
            }
            s.print();
        }
        else {
            std::cout << "ENS:" << s.res.lg.ens << std::endl;
            std::cout << "ANS:" << s.res.lg.ens << std::endl;
        }
    }
}
