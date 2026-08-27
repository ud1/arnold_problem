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
#include <set>
#include <array>

struct Gen {
    size_t gen;
    bool is_triple;
};

struct Pt {
    size_t l1;
    size_t l2;
    bool is_triple;
};

std::vector<size_t> pt_to_vec(Pt pt) {
    if (pt.is_triple) {
        std::vector<size_t> res;
        res.reserve(2);
        res.push_back(pt.l1);
        res.push_back(pt.l2);
        return res;
    }

    std::vector<size_t> res;
    res.reserve(1);
    res.push_back(pt.l1);
    return res;
}

Pt single(size_t l1) {
    Pt res;
    res.l1 = l1;
    res.l2 = static_cast<size_t>(-1);
    res.is_triple = false;
    return res;
}

Pt triple(size_t l1, size_t l2) {
    Pt res;
    res.l1 = l1;
    res.l2 = l2;
    res.is_triple = true;
    return res;
}

struct OMatrix {
    std::vector<std::vector<Pt>> intersections;
    std::vector<size_t> m_indexes;
    size_t n;
    size_t m_count;
};

OMatrix makeOMatrix(std::vector<Gen> gens) {
    if (gens.empty())
        throw std::runtime_error("Empty generator list");

    // A triple generator at g uses positions g, g+1, g+2.  In particular,
    // max(gen)+2 undercounts n when the topmost generator is triple.
    size_t n = 0;
    for (Gen gen : gens)
        n = std::max(n, gen.gen + (gen.is_triple ? 3u : 2u));
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0); // fill 0 1 2 ...
    std::vector<std::vector<Pt>> intersections(n);
    std::vector<size_t> m_indexes;

    for (Gen gen : gens)
    {
        if (!gen.is_triple) {
            size_t first = lines[gen.gen];
            size_t second = lines[gen.gen + 1];

            intersections[first].push_back(single(second));
            intersections[second].push_back(single(first));

            lines[gen.gen] = second;
            lines[gen.gen + 1] = first;
        }
        else {
            size_t first = lines[gen.gen];
            size_t middle = lines[gen.gen + 1];
            size_t second = lines[gen.gen + 2];

            intersections[first].push_back(triple(second, middle));
            intersections[middle].push_back(triple(first, second));
            intersections[second].push_back(triple(first, middle));

            lines[gen.gen] = second;
            lines[gen.gen + 2] = first;
        }
    }

    size_t m_count = 0;
    m_indexes.push_back(0);
    for (size_t i = 1u; i < n; ++i) {
        if (lines[n - 1 - i] > lines[n - i])
            ++m_count;
        m_indexes.push_back(m_count);
    }

    return OMatrix{intersections, m_indexes, n, m_count + 1};
}

std::vector<Gen> gens_str_to_vec(const std::string& s) {
    std::vector<Gen> result;
    std::stringstream ss(s);
    std::string token;

    while (ss >> token) {
        Gen g;
        g.is_triple = false;

        if (!token.empty() && token.back() == '*') {
            g.is_triple = true;
            token.pop_back();
        }

        if (token.empty()) {
            throw std::runtime_error("Invalid string");
        }
        g.gen = std::stoul(token);
        result.push_back(g);
    }

    return result;
}

const double MIN_EDGE = 1.0;
// const double MIN_EDGE = 0.01;
const double MIN_EDGE_FL = 1.0;
const double MIN_ANGLE = 0.01;
const double TRIPLE_EPS = 1e-10;

// y_ij = (x0_j - x0_i)/(m_i - m_j)
// y_ij > y_ik
// (x0_j - x0_i)/(m_i - m_j) > (x0_k - x0_i)/(m_i - m_k)
// (x0_k - x0_i)*(m_i - m_j) - (x0_j - x0_i) * (m_i - m_k) < 0
struct EdgeTerm {
    size_t xi, xj, xk;
    size_t mi, mj, mk;
    double sign;
    bool is_triple;
    EdgeTerm(size_t xi, size_t xj, size_t xk, size_t mi, size_t mj, size_t mk, double sign, bool is_triple)
        : xi(xi),
          xj(xj),
          xk(xk),
          mi(mi),
          mj(mj),
          mk(mk),
          sign(sign),
          is_triple(is_triple) {
    }
};


struct TermsConfig {
    size_t max_steps = 10000000;
    size_t enhance_steps = 1000000;
    double min_decay = 0.00001;
    bool enhance = false;
    bool verbose = false;
    double enhance_angle_func_scale = 10000.0;
};

struct Params {
    std::vector<double> m;
    std::vector<double> x;
};

struct Boost {
    std::vector<size_t> ans_iters;
    std::vector<size_t> ens_iters;

    void inc_ens(size_t i) {
        ens_iters[i]++;
    }
    void dec_ens(size_t i) {
        if (ens_iters[i] > 0)
            ens_iters[i]--;
    }
    double val_ens(size_t i) const {
        size_t v = ens_iters[i];
        if (v == 0)
            return 1.0;

        return 10.0 * (double) v / ((double) v + 100.0) + 1.0;
    }

    void inc_ans(size_t i) {
        ans_iters[i]++;
    }
    void dec_ans(size_t i) {
        if (ans_iters[i] > 0)
            ans_iters[i]--;
    }
    double val_ans(size_t i) const {
        size_t v = ans_iters[i];
        if (v == 0)
            return 1.0;

        return 10.0 * (double) v / ((double) v + 50.0) + 1.0;
    }
};

struct LossAndGrad {
    double loss;
    double pure_loss;
    std::vector<double> grad_m;
    std::vector<double> grad_x;
    bool solved;
    size_t enss, ens, ans;
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
        enss = 0;
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
    std::vector<EdgeTerm> tripleEdgeTerms;
    // tripleEdgeTerms содержит каждую тройную точку трижды (по разу от каждой
    // из трёх прямых); здесь — по одному представителю на точку, для проекции.
    std::vector<EdgeTerm> uniqueTriples;
    std::vector<std::pair<size_t, size_t>> extreme_points_x;
    std::vector<std::pair<size_t, size_t>> extreme_points_m;
    std::vector<size_t> m_indexes;
    double angle_func_scale = 5000.0;
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

typedef void (*compute_loss_and_grad_func)(LossAndGrad &res, const Params &params, const Problem &p, const Boost &b);

void update_boost(const Params &params, const Problem &p, Boost &b) {
    size_t m = params.m.size();
    for (size_t i = 0; i < m-1; ++i) {
        double dm = params.m[i + 1] - params.m[i];
        if (dm > 0.0) {
            b.inc_ans(i);
        }
        else {
            b.dec_ans(i);
        }
    }

    size_t i = 0;
    for (auto &t : p.edgeTerms) {
        double dm = ((params.x[t.xj] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mk]) - (params.x[t.xk] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mj]))*t.sign;
        if (dm >= 0.0) {
            b.inc_ens(i);
        }
        else {
            b.dec_ens(i);
        }
        ++i;
    }
}

void compute_loss_and_grad_initial(LossAndGrad &res, const Params &params, const Problem &p, const Boost &b) {
    res.reset();

    size_t m = params.m.size();
    for (size_t i = 0; i < m-1; ++i) {
        double dm = params.m[i + 1] - params.m[i];
        if (dm > 0.0) {
            res.solved = false;
            res.ans++;
            res.pure_loss += dm * dm * 5000.0;
        }
        dm += MIN_ANGLE;
        if (dm > 0.0) {
            double boost = b.val_ans(i);
            res.loss += dm*dm * p.angle_func_scale*boost;
            double g = 2.0*dm * p.angle_func_scale*boost;
            res.grad_m[i] -= g;
            res.grad_m[i + 1] += g;
        }
    }

    size_t i = 0;
    for (auto &t : p.edgeTerms) {
        double dm = ((params.x[t.xj] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mk]) - (params.x[t.xk] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mj]))*t.sign;
        if (t.is_triple) {
            if (std::abs(dm) > TRIPLE_EPS) {
                res.solved = false;
                res.ens++;
                res.enss++;
                res.pure_loss += dm * dm;
            }
        }
        else {
            if (dm >= 0.0) {
                res.solved = false;
                res.ens++;
                res.pure_loss += dm * dm;
            }
            dm += MIN_EDGE;
        }

        if (dm > 0.0 || t.is_triple) {
            double boost = b.val_ens(i);
            res.loss += dm*dm*boost;

            double val = dm * 2.0 * boost;
            double b1 = (params.x[t.xj] - params.x[t.xi]) * t.sign;
            double b2 = (params.x[t.xk] - params.x[t.xi]) * t.sign;

            res.grad_m[t.mi] += val * (b1 - b2);
            res.grad_m[t.mj] += val * b2;
            res.grad_m[t.mk] -= val * b1;

            double a = (params.m[t.mi] - params.m[t.mk]) * t.sign;
            double b = (params.m[t.mi] - params.m[t.mj]) * t.sign;

            res.grad_x[t.xi] += val * (b - a);
            res.grad_x[t.xj] += val * a;
            res.grad_x[t.xk] -= val * b;
        }

        ++i;
    }
}

void print_ens(const Params &params, const Problem &p) {
    for (auto &t : p.edgeTerms) {
        double dm = ((params.x[t.xj] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mk]) - (params.x[t.xk] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mj]))*t.sign;
        if (t.is_triple) {
            if (std::abs(dm) > TRIPLE_EPS) {
                std::cout << "ENS_P*: " << t.xi << " " << t.xj << " " << t.xk << " " << std::abs(dm) << std::endl;
            }
        }
        else {
            if (dm >= 0.0) {
                std::cout << "ENS_P: " << t.xi << " " << t.xj << " " << t.xk << " " << dm << std::endl;
            }
        }
    }

    size_t m = params.m.size();
    for (size_t i = 0; i < m-1; ++i) {
        double dm = params.m[i + 1] - params.m[i];
        if (dm > 0.0) {
            std::cout << "ANS_P: " << i << " " << (i + 1) << " " << dm << std::endl;
        }
    }
}

void compute_loss_and_grad_enhance(LossAndGrad &res, const Params &params, const Problem &p, const Boost &b) {
    res.reset();

    if (params.m.size() < 2)
        return;

    size_t m = params.m.size();
    for (size_t i = 0; i < m; ++i) {
        if (i + 1 < m && params.m[i] < params.m[i + 1]) {
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
        double dm = ((params.x[t.xj] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mk]) - (params.x[t.xk] - params.x[t.xi]) * (params.m[t.mi] - params.m[t.mj]))*t.sign;
        if (t.is_triple) {
            // if (std::abs(dm) > TRIPLE_EPS) {
            //     res.solved = false;
            //     res.ens++;
            // }
        }
        else {
            if (dm >= 0.0) {
                res.solved = false;
                res.ens++;
            }
        }

        size_t xi = t.xi;
        size_t xj = t.xj;
        size_t xk = t.xk;

        size_t mi = t.mi;
        size_t mj = t.mj;
        size_t mk = t.mk;

        auto &ep_x = p.extreme_points_x[xi];
        auto &ep_m = p.extreme_points_m[xi];
        size_t xj1 = ep_x.first;
        size_t xk1 = ep_x.second;
        size_t mj1 = ep_m.first;
        size_t mk1 = ep_m.second;

        // ----------------------
        // numerator: scaleCoef for (i, j1, k1)
        // ----------------------
        double dm1_ij = params.m[mi] - params.m[mj1];
        double dm1_ik = params.m[mi] - params.m[mk1];

        double x1_ij = (params.x[xj1] * params.m[mi] - params.x[xi] * params.m[mj1]) / dm1_ij;
        double y1_ij = (params.x[xj1] - params.x[xi]) / dm1_ij;

        double x1_ik = (params.x[xk1] * params.m[mi] - params.x[xi] * params.m[mk1]) / dm1_ik;
        double y1_ik = (params.x[xk1] - params.x[xi]) / dm1_ik;

        double dx1 = x1_ik - x1_ij;
        double dy1 = y1_ik - y1_ij;

        double scaleCoef = dx1 * dx1 + dy1 * dy1;

        // ----------------------
        // denominator: len2 for (i, j, k)
        // ----------------------
        double dm_ij = params.m[mi] - params.m[mj];
        double dm_ik = params.m[mi] - params.m[mk];

        double x_ij = (params.x[xj] * params.m[mi] - params.x[xi] * params.m[mj]) / dm_ij;
        double y_ij = (params.x[xj] - params.x[xi]) / dm_ij;

        double x_ik = (params.x[xk] * params.m[mi] - params.x[xi] * params.m[mk]) / dm_ik;
        double y_ik = (params.x[xk] - params.x[xi]) / dm_ik;

        double dx = x_ik - x_ij;
        double dy = y_ik - y_ij;

        double len2 = dx * dx + dy * dy;

        if (!t.is_triple)
        {
            double l2 = len2 / scaleCoef;
            if (res.min_len > l2)
                res.min_len = l2;
            if (res.max_len < l2)
                res.max_len = l2;
        }

        double coeff_den;
        double coeff_num;
        if (t.is_triple) {
            res.loss += len2 / scaleCoef;

            double inv_scale = 1.0 / scaleCoef;
            double inv_scale2 = inv_scale * inv_scale;

            coeff_den =  2.0 * inv_scale;
            coeff_num = -2.0 * len2 * inv_scale2;
        }
        else {
            res.loss += scaleCoef / len2;

            double inv_len2 = 1.0 / len2;
            double inv_len4 = 1.0 / (len2 * len2);

            coeff_num =  2.0 * inv_len2;
            coeff_den = -2.0 * scaleCoef * inv_len4;
        }

        // ----------------------
        // gradients wrt m, denominator (i,j,k)
        // ----------------------
        double d_dx_dmi = params.m[mk] * (params.x[xi] - params.x[xk]) / (dm_ik * dm_ik)
                        - params.m[mj] * (params.x[xi] - params.x[xj]) / (dm_ij * dm_ij);
        double d_dy_dmi = (params.x[xi] - params.x[xk]) / (dm_ik * dm_ik)
                        - (params.x[xi] - params.x[xj]) / (dm_ij * dm_ij);

        double d_dx_dmj = params.m[mi] * (params.x[xi] - params.x[xj]) / (dm_ij * dm_ij);
        double d_dy_dmj = (params.x[xi] - params.x[xj]) / (dm_ij * dm_ij);

        double d_dx_dmk = params.m[mi] * (params.x[xk] - params.x[xi]) / (dm_ik * dm_ik);
        double d_dy_dmk = (params.x[xk] - params.x[xi]) / (dm_ik * dm_ik);

        // ----------------------
        // gradients wrt m, numerator (i,j1,k1)
        // ----------------------
        double d_dx1_dmi = params.m[mk1] * (params.x[xi] - params.x[xk1]) / (dm1_ik * dm1_ik)
                         - params.m[mj1] * (params.x[xi] - params.x[xj1]) / (dm1_ij * dm1_ij);
        double d_dy1_dmi = (params.x[xi] - params.x[xk1]) / (dm1_ik * dm1_ik)
                         - (params.x[xi] - params.x[xj1]) / (dm1_ij * dm1_ij);

        double d_dx1_dmj1 = params.m[mi] * (params.x[xi] - params.x[xj1]) / (dm1_ij * dm1_ij);
        double d_dy1_dmj1 = (params.x[xi] - params.x[xj1]) / (dm1_ij * dm1_ij);

        double d_dx1_dmk1 = params.m[mi] * (params.x[xk1] - params.x[xi]) / (dm1_ik * dm1_ik);
        double d_dy1_dmk1 = (params.x[xk1] - params.x[xi]) / (dm1_ik * dm1_ik);

        // ----------------------
        // gradients wrt x, denominator (i,j,k)
        // ----------------------
        double d_dx_dxi = -params.m[mk] / dm_ik + params.m[mj] / dm_ij;
        double d_dy_dxi = -1.0 / dm_ik + 1.0 / dm_ij;

        double d_dx_dxj = -params.m[mi] / dm_ij;
        double d_dy_dxj = -1.0 / dm_ij;

        double d_dx_dxk = params.m[mi] / dm_ik;
        double d_dy_dxk = 1.0 / dm_ik;

        // ----------------------
        // gradients wrt x, numerator (i,j1,k1)
        // ----------------------
        double d_dx1_dxi = -params.m[mk1] / dm1_ik + params.m[mj1] / dm1_ij;
        double d_dy1_dxi = -1.0 / dm1_ik + 1.0 / dm1_ij;

        double d_dx1_dxj1 = -params.m[mi] / dm1_ij;
        double d_dy1_dxj1 = -1.0 / dm1_ij;

        double d_dx1_dxk1 = params.m[mi] / dm1_ik;
        double d_dy1_dxk1 = 1.0 / dm1_ik;

        // ----------------------
        // accumulate gradients
        // ----------------------
        res.grad_m[mi]  += coeff_num * (dx1 * d_dx1_dmi  + dy1 * d_dy1_dmi)
                        + coeff_den * (dx  * d_dx_dmi   + dy  * d_dy_dmi);
        res.grad_m[mj]  += coeff_den * (dx * d_dx_dmj + dy * d_dy_dmj);
        res.grad_m[mk]  += coeff_den * (dx * d_dx_dmk + dy * d_dy_dmk);
        res.grad_m[mj1] += coeff_num * (dx1 * d_dx1_dmj1 + dy1 * d_dy1_dmj1);
        res.grad_m[mk1] += coeff_num * (dx1 * d_dx1_dmk1 + dy1 * d_dy1_dmk1);

        res.grad_x[xi]  += coeff_num * (dx1 * d_dx1_dxi  + dy1 * d_dy1_dxi)
                        + coeff_den * (dx  * d_dx_dxi   + dy  * d_dy_dxi);
        res.grad_x[xj]  += coeff_den * (dx * d_dx_dxj + dy * d_dy_dxj);
        res.grad_x[xk]  += coeff_den * (dx * d_dx_dxk + dy * d_dy_dxk);
        res.grad_x[xj1] += coeff_num * (dx1 * d_dx1_dxj1 + dy1 * d_dy1_dxj1);
        res.grad_x[xk1] += coeff_num * (dx1 * d_dx1_dxk1 + dy1 * d_dy1_dxk1);
    }
}

void init(const OMatrix &o, Params &params, Problem &p) {
    size_t n = o.n;
    size_t mn = o.m_count;

    params.m.clear();
    for (size_t i = 0; i < mn; ++i) {
        params.m.push_back((double) mn * 0.5 - (double) i - 0.1);
    }

    params.x.resize(n, 0);
    p.edgeTerms.clear();
    p.tripleEdgeTerms.clear();
    p.extreme_points_x.clear();
    p.extreme_points_m.clear();
    for (size_t i = 0; i < o.intersections.size(); ++i) {
        auto &line = o.intersections[i];

        if (line.size() >= 2) {
            for (size_t ind = 0; ind < line.size() - 1; ++ind) {
                Pt pj = line[ind];
                Pt pk = line[ind + 1];

                for (size_t j : pt_to_vec(pj)) {
                    for (size_t k : pt_to_vec(pk)) {
                        double sign = (i > j) == (i > k) ? +1.0 : -1.0;
                        p.edgeTerms.emplace_back(i, j, k, o.m_indexes[i], o.m_indexes[j], o.m_indexes[k], sign, false);
                    }
                }

            }
        }

        for (size_t ind = 0; ind < line.size(); ++ind) {
            Pt pt = line[ind];
            if (pt.is_triple) {
                EdgeTerm edge_term{
                    i, pt.l1, pt.l2,
                    o.m_indexes[i], o.m_indexes[pt.l1], o.m_indexes[pt.l2],
                    1.0, true
                };
                p.edgeTerms.push_back(edge_term);
                p.tripleEdgeTerms.push_back(edge_term);
            }
        }

        p.extreme_points_x.emplace_back(line[0].l1, line[line.size() - 1].l1);
        p.extreme_points_m.emplace_back(o.m_indexes[line[0].l1], o.m_indexes[line[line.size() - 1].l1]);
    }

    p.uniqueTriples.clear();
    std::set<std::array<size_t, 3>> seen_triples;
    for (const auto &t : p.tripleEdgeTerms) {
        std::array<size_t, 3> key{t.xi, t.xj, t.xk};
        std::sort(key.begin(), key.end());
        if (seen_triples.insert(key).second)
            p.uniqueTriples.push_back(t);
    }
    p.m_indexes = o.m_indexes;
}

// double eval_loss_only(const Params &params, const Problem &p, compute_loss_and_grad_func compute_loss_and_grad, Boost &b) {
//     LossAndGrad tmp;
//     tmp.init(params);
//     compute_loss_and_grad(tmp, params, p, b);
//     return tmp.loss;
// }
//
// void check_grad(const Params &params, const Problem &p, compute_loss_and_grad_func compute_loss_and_grad, Boost &b) {
//     const double eps = 1e-7;
//
//     LossAndGrad lg;
//     lg.init(params);
//     compute_loss_and_grad(lg, params, p, b);
//
//     for (size_t i = 0; i < params.m.size(); ++i) {
//         Params p1 = params;
//         Params p2 = params;
//
//         p1.m[i] += eps;
//         p2.m[i] -= eps;
//
//         double f1 = eval_loss_only(p1, p, compute_loss_and_grad, b);
//         double f2 = eval_loss_only(p2, p, compute_loss_and_grad, b);
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
//         double f1 = eval_loss_only(p1, p, compute_loss_and_grad, b);
//         double f2 = eval_loss_only(p2, p, compute_loss_and_grad, b);
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

void project_one_triple(
    std::vector<double>& delta_m,
    std::vector<double>& delta_x,
    const Params& params,
    const EdgeTerm& t)
{
    size_t xi = t.xi;
    size_t xj = t.xj;
    size_t xk = t.xk;

    size_t mi = t.mi;
    size_t mj = t.mj;
    size_t mk = t.mk;

    // --- compute gradient of constraint c ---

    double b1 = (params.x[xj] - params.x[xi]) * t.sign;
    double b2 = (params.x[xk] - params.x[xi]) * t.sign;

    double gc_m_i =  (b1 - b2);
    double gc_m_j =  b2;
    double gc_m_k = -b1;

    double a = (params.m[mi] - params.m[mk]) * t.sign;
    double b = (params.m[mi] - params.m[mj]) * t.sign;

    double gc_x_i =  (b - a);
    double gc_x_j =  a;
    double gc_x_k = -b;

    // --- compute dot( gc , delta ) ---
    double dot = 0.0;
    dot += gc_m_i * delta_m[mi];
    dot += gc_m_j * delta_m[mj];
    dot += gc_m_k * delta_m[mk];

    dot += gc_x_i * delta_x[xi];
    dot += gc_x_j * delta_x[xj];
    dot += gc_x_k * delta_x[xk];

    // --- compute ||gc||^2 ---
    double norm2 = 0.0;
    norm2 += gc_m_i * gc_m_i;
    norm2 += gc_m_j * gc_m_j;
    norm2 += gc_m_k * gc_m_k;

    norm2 += gc_x_i * gc_x_i;
    norm2 += gc_x_j * gc_x_j;
    norm2 += gc_x_k * gc_x_k;

    if (norm2 < 1e-30)
        return;

    double coeff = dot / norm2;

    // --- subtract projection ---
    delta_m[mi] -= coeff * gc_m_i;
    delta_m[mj] -= coeff * gc_m_j;
    delta_m[mk] -= coeff * gc_m_k;

    delta_x[xi] -= coeff * gc_x_i;
    delta_x[xj] -= coeff * gc_x_j;
    delta_x[xk] -= coeff * gc_x_k;
}

void enforce_triple_exact(Params& params, const EdgeTerm& t)
{
    size_t i = t.xi;
    size_t j = t.xj;
    size_t k = t.xk;

    auto intersect = [&](size_t a, size_t b)
    {
        double dm = params.m[a] - params.m[b];
        if (std::abs(dm) < 1e-14)
            return std::pair<double,double>(0.0, 0.0); // почти параллельны

        double y = (params.x[b] - params.x[a]) / dm;
        double x = params.x[a] + params.m[a] * y;
        return std::pair<double,double>(x, y);
    };

    auto p_ij = intersect(i, j);
    auto p_ik = intersect(i, k);
    auto p_jk = intersect(j, k);

    // центр трёх пересечений
    double xp = (p_ij.first  + p_ik.first  + p_jk.first)  / 3.0;
    double yp = (p_ij.second + p_ik.second + p_jk.second) / 3.0;

    // теперь заставляем все три прямые проходить через (xp, yp)
    params.x[i] = xp - params.m[i] * yp;
    params.x[j] = xp - params.m[j] * yp;
    params.x[k] = xp - params.m[k] * yp;
}

// Решает симметричную систему G*y = rhs гауссом с частичным выбором ведущего
// элемента. G и rhs портятся; решение остаётся в rhs.
// Матрица Грама тройных ограничений вырожденная по построению: пучковые
// направления {1, m} лежат в ядре матрицы ограничений, поэтому её ранг
// не превосходит n-2, а строк может быть больше. Система при этом совместна
// (правая часть лежит в образе), так что линейно зависимые строки можно
// пропускать (их y = 0) — но отличать их надо ОТНОСИТЕЛЬНЫМ порогом:
// абсолютный порог пропускает пивоты уровня шума округления, деление на них
// ломает решение, и проекция перестаёт зануливать невязку.
void solve_gram_system(size_t T, std::vector<double> &G, std::vector<double> &rhs)
{
    double max_diag = 0.0;
    for (size_t d = 0; d < T; ++d)
        max_diag = std::max(max_diag, std::abs(G[d*T + d]));
    const double pivot_threshold = max_diag * 1e-10;
    if (max_diag == 0.0) {
        for (size_t d = 0; d < T; ++d)
            rhs[d] = 0.0;
        return;
    }

    for (size_t col = 0; col < T; ++col) {
        size_t piv = col;
        for (size_t row = col + 1; row < T; ++row)
            if (std::abs(G[row*T + col]) > std::abs(G[piv*T + col]))
                piv = row;
        if (std::abs(G[piv*T + col]) <= pivot_threshold) {
            G[col*T + col] = 0.0;
            continue;
        }
        if (piv != col) {
            for (size_t c2 = col; c2 < T; ++c2)
                std::swap(G[piv*T + c2], G[col*T + c2]);
            std::swap(rhs[piv], rhs[col]);
        }
        double inv = 1.0 / G[col*T + col];
        for (size_t row = col + 1; row < T; ++row) {
            double f = G[row*T + col] * inv;
            if (f == 0.0)
                continue;
            for (size_t c2 = col; c2 < T; ++c2)
                G[row*T + c2] -= f * G[col*T + c2];
            rhs[row] -= f * rhs[col];
        }
    }
    for (size_t col = T; col-- > 0; ) {
        if (G[col*T + col] == 0.0) {
            rhs[col] = 0.0;
            continue;
        }
        double s = rhs[col];
        for (size_t c2 = col + 1; c2 < T; ++c2)
            s -= G[col*T + c2] * rhs[c2];
        rhs[col] = s / G[col*T + col];
    }
}

// Точное восстановление всех тройных точек сразу: ньютоновская минимально-
// нормовая коррекция ПО ОБОИМ наборам переменных (m и x) на многообразии
//   c_t = (x_j - x_i)(m_i - m_k) - (x_k - x_i)(m_i - m_j) = 0.
// Чинить только x (как enforce_triple_exact) при большом числе тройных точек
// нельзя в принципе: при фиксированных m система A(m)x = 0 из T ограничений
// имеет ранг n-2 уже при T >= n-2, и её решения — ровно пучок x_i = X - m_i*Y
// (все прямые через одну точку). Не-пучковые решения существуют лишь при
// специальных m, поэтому коррекция обязана подстраивать и наклоны.
// Ограничение билинейное — Ньютон сходится квадратично, 2-3 итерации.
// Каждая итерация: Delta = -J^T (J J^T)^{-1} c по полному якобиану
//   dc/dm_i = x_j - x_k,   dc/dm_j = x_k - x_i,   dc/dm_k = x_i - x_j,
//   dc/dx_i = m_k - m_j,   dc/dx_j = m_i - m_k,   dc/dx_k = m_j - m_i.
void enforce_all_triples_exact(Params& params, const Problem& p)
{
    const auto &triples = p.uniqueTriples;
    const size_t T = triples.size();
    if (T == 0)
        return;

    static std::vector<double> Jm, Jx;  // T x 3
    static std::vector<double> G;       // T x T: матрица Грама J J^T
    static std::vector<double> mu;      // невязки -> множители
    Jm.resize(T * 3);
    Jx.resize(T * 3);
    mu.resize(T);

    double prev_resid = 1e300;
    for (int pass = 0; pass < 8; ++pass) {
        double xscale = 0.0;
        for (double v : params.x)
            xscale = std::max(xscale, std::abs(v));
        for (double v : params.m)
            xscale = std::max(xscale, std::abs(v));

        // Строки якобиана нормируем на единичную длину (улучшает пивотинг и
        // делает относительный порог в solve_gram_system осмысленным);
        // невязка делится на ту же норму.
        double max_resid = 0.0;
        for (size_t t = 0; t < T; ++t) {
            const EdgeTerm &e = triples[t];
            Jm[t*3 + 0] = params.x[e.xj] - params.x[e.xk];
            Jm[t*3 + 1] = params.x[e.xk] - params.x[e.xi];
            Jm[t*3 + 2] = params.x[e.xi] - params.x[e.xj];
            Jx[t*3 + 0] = params.m[e.mk] - params.m[e.mj];
            Jx[t*3 + 1] = params.m[e.mi] - params.m[e.mk];
            Jx[t*3 + 2] = params.m[e.mj] - params.m[e.mi];
            double rn = 0.0;
            for (int u = 0; u < 3; ++u)
                rn += Jm[t*3 + u] * Jm[t*3 + u] + Jx[t*3 + u] * Jx[t*3 + u];
            rn = std::sqrt(rn);
            double c = (params.x[e.xj] - params.x[e.xi]) * (params.m[e.mi] - params.m[e.mk])
                     - (params.x[e.xk] - params.x[e.xi]) * (params.m[e.mi] - params.m[e.mj]);
            if (rn > 1e-150) {
                for (int u = 0; u < 3; ++u) {
                    Jm[t*3 + u] /= rn;
                    Jx[t*3 + u] /= rn;
                }
                c /= rn;
            }
            mu[t] = c;
            max_resid = std::max(max_resid, std::abs(c));
        }

        // Сошлись до уровня округления — или упёрлись в его пол.
        if (max_resid <= 1e-13 * (xscale + 1.0) || max_resid >= 0.5 * prev_resid)
            break;
        prev_resid = max_resid;

        G.assign(T * T, 0.0);
        for (size_t a = 0; a < T; ++a) {
            const size_t xa[3] = {triples[a].xi, triples[a].xj, triples[a].xk};
            const size_t ma[3] = {triples[a].mi, triples[a].mj, triples[a].mk};

            for (size_t b = a; b < T; ++b) {
                const size_t xb[3] = {triples[b].xi, triples[b].xj, triples[b].xk};
                const size_t mb[3] = {triples[b].mi, triples[b].mj, triples[b].mk};

                double s = 0.0;

                // 1. Перекрытие по переменным x (интерсептам)
                for (int u = 0; u < 3; ++u) {
                    for (int v = 0; v < 3; ++v) {
                        if (xa[u] == xb[v]) {
                            s += Jx[a*3 + u] * Jx[b*3 + v];
                        }
                    }
                }

                // 2. Перекрытие по переменным m (наклонам)
                for (int u = 0; u < 3; ++u) {
                    for (int v = 0; v < 3; ++v) {
                        if (ma[u] == mb[v]) {
                            s += Jm[a*3 + u] * Jm[b*3 + v];
                        }
                    }
                }

                G[a*T + b] = s;
                G[b*T + a] = s; // Матрица симметрична
            }
        }

        solve_gram_system(T, G, mu);

        for (size_t t = 0; t < T; ++t) {
            const EdgeTerm &e = triples[t];
            params.m[e.mi] -= mu[t] * Jm[t*3 + 0];
            params.m[e.mj] -= mu[t] * Jm[t*3 + 1];
            params.m[e.mk] -= mu[t] * Jm[t*3 + 2];
            params.x[e.xi] -= mu[t] * Jx[t*3 + 0];
            params.x[e.xj] -= mu[t] * Jx[t*3 + 1];
            params.x[e.xk] -= mu[t] * Jx[t*3 + 2];
        }
    }

    // Пиновка калибровки. Сдвиги x вдоль 1 и вдоль m и общий масштаб x не
    // меняют ни лосс enhance-фазы (он масштабно-инвариантен), ни ограничения:
    // пучок x_i = X - m_i*Y (все прямые через точку (X,Y)) удовлетворяет всем
    // c_t при любых m. Повторные проекции при меняющихся m дрейфуют вдоль этих
    // плоских направлений к пучку — конфигурация схлопывается в точку и теряет
    // абсолютную точность. Явно выкидываем пучковые компоненты и нормируем
    // масштаб; лосс при этом не меняется вовсе.
    // Удаляем пучковые компоненты x_i -> x_i + C1 + C2 * m_i
    const size_t n = params.x.size();

    double sum_m = 0.0, sum_m2 = 0.0, sum_x = 0.0, sum_xm = 0.0;
    for (size_t i = 0; i < n; ++i) {
        size_t mi = p.m_indexes[i];
        double m_val = params.m[mi];
        double x_val = params.x[i];
        sum_m += m_val;
        sum_m2 += m_val * m_val;
        sum_x += x_val;
        sum_xm += x_val * m_val;
    }

    double N_d = (double)n;
    double det = N_d * sum_m2 - sum_m * sum_m;
    double c1 = 0.0, c2 = 0.0;

    if (std::abs(det) > 1e-30) {
        c1 = (sum_x * sum_m2 - sum_xm * sum_m) / det;
        c2 = (N_d * sum_xm - sum_m * sum_x) / det;
    }

    double norm2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        size_t mi = p.m_indexes[i];
        params.x[i] -= (c1 + c2 * params.m[mi]);
        norm2 += params.x[i] * params.x[i];
    }

    const double target = 10.0 * std::sqrt((double) n);
    if (norm2 > 1e-30) {
        double scale = target / std::sqrt(norm2);
        for (size_t i = 0; i < n; ++i)
            params.x[i] *= scale;
    }
}

// Проекция градиента на касательное пространство ограничений тройных точек
// (по всем переменным m и x). Роль — НЕ удержание точек (это делает точная
// проекция x в enforce_all_triples_exact), а согласованность линейного поиска:
// нормальная к ограничениям компонента градиента всё равно уничтожается
// проекционным шагом, но без этой функции она входит в g2, тест Армихо требует
// недостижимого спада лосса, и step схлопывается до нуля.
void project_grad_all_triples(LossAndGrad &lg, const Params &params, const Problem &p)
{
    const auto &triples = p.uniqueTriples;
    const size_t T = triples.size();
    if (T == 0)
        return;

    // Строка якобиана t: 6 ненулевых элементов (по m и x трёх прямых).
    static std::vector<double> Jm, Jx; // T x 3
    static std::vector<double> G;      // T x T
    static std::vector<double> mu;
    Jm.resize(T * 3);
    Jx.resize(T * 3);
    G.assign(T * T, 0.0);
    mu.resize(T);

    for (size_t t = 0; t < T; ++t) {
        const EdgeTerm &e = triples[t];
        Jm[t*3 + 0] = params.x[e.xj] - params.x[e.xk]; // dc/dm_i
        Jm[t*3 + 1] = params.x[e.xk] - params.x[e.xi]; // dc/dm_j
        Jm[t*3 + 2] = params.x[e.xi] - params.x[e.xj]; // dc/dm_k
        Jx[t*3 + 0] = params.m[e.mk] - params.m[e.mj]; // dc/dx_i
        Jx[t*3 + 1] = params.m[e.mi] - params.m[e.mk]; // dc/dx_j
        Jx[t*3 + 2] = params.m[e.mj] - params.m[e.mi]; // dc/dx_k
        double rn = 0.0;
        for (int u = 0; u < 3; ++u)
            rn += Jm[t*3 + u] * Jm[t*3 + u] + Jx[t*3 + u] * Jx[t*3 + u];
        rn = std::sqrt(rn);
        if (rn > 1e-150) {
            for (int u = 0; u < 3; ++u) {
                Jm[t*3 + u] /= rn;
                Jx[t*3 + u] /= rn;
            }
        }
        mu[t] = Jm[t*3 + 0] * lg.grad_m[e.mi] + Jm[t*3 + 1] * lg.grad_m[e.mj] + Jm[t*3 + 2] * lg.grad_m[e.mk]
              + Jx[t*3 + 0] * lg.grad_x[e.xi] + Jx[t*3 + 1] * lg.grad_x[e.xj] + Jx[t*3 + 2] * lg.grad_x[e.xk];
    }

    for (size_t a = 0; a < T; ++a) {
        const size_t la[3] = {triples[a].xi, triples[a].xj, triples[a].xk};
        for (size_t b = a; b < T; ++b) {
            const size_t lb[3] = {triples[b].xi, triples[b].xj, triples[b].xk};
            double s = 0.0;
            for (int u = 0; u < 3; ++u)
                for (int v = 0; v < 3; ++v)
                    if (la[u] == lb[v])
                        s += Jm[a*3 + u] * Jm[b*3 + v] + Jx[a*3 + u] * Jx[b*3 + v];
            G[a*T + b] = s;
            G[b*T + a] = s;
        }
    }

    solve_gram_system(T, G, mu);

    for (size_t t = 0; t < T; ++t) {
        const EdgeTerm &e = triples[t];
        lg.grad_m[e.mi] -= mu[t] * Jm[t*3 + 0];
        lg.grad_m[e.mj] -= mu[t] * Jm[t*3 + 1];
        lg.grad_m[e.mk] -= mu[t] * Jm[t*3 + 2];
        lg.grad_x[e.xi] -= mu[t] * Jx[t*3 + 0];
        lg.grad_x[e.xj] -= mu[t] * Jx[t*3 + 1];
        lg.grad_x[e.xk] -= mu[t] * Jx[t*3 + 2];
    }
}

Results solve(size_t iters, const Problem &p, Params &params, compute_loss_and_grad_func compute_loss_and_grad, Boost &b, bool enhance) {
    LossAndGrad lg1, lg2;
    lg1.init(params);
    lg2.init(params);

    LossAndGrad *prev_lg = &lg1;
    LossAndGrad *new_lg  = &lg2;

    // init() only sizes the gradient arrays.  Keep both work buffers in a
    // defined state even before the first candidate has been evaluated.
    prev_lg->reset();
    new_lg->reset();

    Results results;
    Params sp;

    double step = 0.01;

    update_boost(params, p, b);
    compute_loss_and_grad(*prev_lg, params, p, b);
    if (enhance)
        project_grad_all_triples(*prev_lg, params, p);

    for (; results.iters < iters; ++results.iters) {
        double g2 = prev_lg->g2();

        if (g2 < 1e-30) {
            results.lg = *prev_lg;
            return results;
        }

        sp = params;

        bool accepted = false;
        int bt = 0;
        for (; bt < 1000; ++bt) {
            for (size_t i = 0; i < params.m.size(); ++i) {
                params.m[i] = sp.m[i] - step * prev_lg->grad_m[i];
            }
            for (size_t i = 0; i < params.x.size(); ++i) {
                params.x[i] = sp.x[i] - step * prev_lg->grad_x[i];
            }
            if (enhance) {
                enforce_all_triples_exact(params, p);
            }

            compute_loss_and_grad(*new_lg, params, p, b);
            if (enhance)
                project_grad_all_triples(*new_lg, params, p);

            if (!enhance || new_lg->solved) {
                if (new_lg->loss - prev_lg->loss <= -0.5 * step * g2) {
                    accepted = true;
                    break;
                }
            }

            ++results.iters;
            step *= 0.5;
            if (step < 1e-100) {
                params = sp;
                results.lg = *prev_lg;
                // check_grad(params, p, compute_loss_and_grad, b);
                return results;
            }
        }

        if (!accepted) {
            params = sp;
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

        if (bt < 3) {
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

        if (!enhance && prev_lg->solved) {
            break;
        }
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

    Stretch(const std::vector<Gen> &gens, const TermsConfig &terms_config) {
        o = makeOMatrix(gens);
        config = terms_config;
        init(o, params, problem);
        problem.angle_func_scale2 = terms_config.enhance_angle_func_scale;
    }

    // The penalty phase approaches triple equalities only asymptotically.
    // Once all remaining violations are triple equalities, try an exact
    // projection on a copy.  Commit it only when *all* constraints still
    // hold; otherwise the penalty trajectory is left untouched.
    bool try_finish_with_exact_triples(const LossAndGrad &lg, const Boost &b) {
        if (lg.ans != 0 || lg.ens == 0 || lg.ens != lg.enss)
            return false;

        Params projected_params = params;
        enforce_all_triples_exact(projected_params, problem);

        LossAndGrad projected_lg;
        projected_lg.init(projected_params);
        compute_loss_and_grad_initial(projected_lg, projected_params, problem, b);
        if (!projected_lg.solved)
            return false;

        params = std::move(projected_params);
        res.lg = std::move(projected_lg);
        return true;
    }

    void stretch() {
        Boost b;
        b.ens_iters.resize(problem.edgeTerms.size(), 0);
        b.ans_iters.resize(params.m.size() > 0 ? params.m.size() - 1 : 0, 0.0);

        std::vector<double> values;
        const size_t ITERS = 20000;
        for (;;) {
            Results results = solve(ITERS, problem, params, compute_loss_and_grad_initial, b, false);
            res.iters += results.iters;
            res.lg = results.lg;
            if (results.lg.solved) {
                return;
            }

            if (try_finish_with_exact_triples(results.lg, b)) {
                return;
            }

            if (results.iters < ITERS) {
                return;
            }

            if (res.iters >= config.max_steps) {
                return;
            }

            if (config.verbose)
                std::cout << results.lg.pure_loss << " ENS " << results.lg.ens << " ANS " << results.lg.ans << " " << results.lg.g2() << std::endl;

            values.push_back(res.lg.pure_loss);
            if (values.size() >= 5) {
                int start = (int) values.size() - 5;
                double max_v = *std::max_element(values.begin() + start, values.end());
                double min_v = *std::min_element(values.begin() + start, values.end());
                double dv = (max_v - min_v) / (max_v + 0.00001);

                if (dv < config.min_decay) {
                    break;
                }
            }
        }
    }

    void enhance() {
        Boost b;
        b.ens_iters.resize(problem.edgeTerms.size(), 0.0);
        b.ans_iters.resize(o.n - 1, 0.0);

        retransform_m(params);
        retransform_x(params);

        const size_t ITERS = 100000;
        const size_t MAX_I = (config.enhance_steps + ITERS - 1) / ITERS;
        for (size_t i = 0; i < MAX_I; ++i) {
            Results results = solve(ITERS, problem, params, compute_loss_and_grad_enhance, b, true);
            res.iters += results.iters;

            if (config.verbose)
                std::cout << "E " << results.lg.loss << " " << res.iters << std::endl;

            res.lg = results.lg;
            if (results.iters < ITERS) {
                break;
            }
        }

        retransform_x(params);
    }

    void print() {
        size_t n = params.x.size();
        for (size_t i = 0; i < n; ++i) {
            double x = params.x[i];
            double m = params.m[o.m_indexes[i]];

            if (std::abs(m) < 1e-8) {
                m = std::copysign(1e-8, m);
                std::cout << "ERR: M was corrected" << std::endl;
            }

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
        std::cout << "STEPS:" << s.res.iters << std::endl;
        // std::cout << "N:" << s.o.n << std::endl;

        if (s.res.lg.solved) {
            s.print();

            if (terms_config.enhance) {
                s.enhance();
                std::cout << "VAL: " << s.res.lg.loss << " VAL_RATIO: " << std::sqrt(s.res.lg.max_len / s.res.lg.min_len) << " " << std::sqrt(s.res.lg.max_len) << std::endl;
            }
            s.print();
        }
        else {
            std::cout << "PVAL:" << s.res.lg.pure_loss << std::endl;
            std::cout << "ENS:" << s.res.lg.ens << std::endl;
            std::cout << "ANS:" << s.res.lg.ans << std::endl;

            print_ens(s.params, s.problem);

            retransform_m(s.params);
            retransform_x(s.params);
            s.print();
        }
    }
}
