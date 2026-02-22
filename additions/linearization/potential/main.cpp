#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <limits>

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
const double MIN_ANGLE = 0.001;

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
        double result = raw(params) + MIN_EDGE;

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
        double result = raw(params) + MIN_ANGLE;
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
    std::vector<double> params1;
    std::vector<double> params2;
    std::vector<double> grad;

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

        params1.resize(params.size());
        params2.resize(params.size());

        auto m = [](size_t i){return i;};
        auto b = [n](size_t i){return i + n;};


        for (size_t i = 0; i < o.n-1; ++i) {
            angleTerms.emplace_back(m(i), m(i + 1));
        }

        for (size_t i = 0; i < o.n; ++i) {
            auto &line = o.intersections[i];

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

        return sqrt(result);
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

int main() {
    OMatrix o = makeOMatrix(gens_str_to_vec("1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 3 5 7 9 11 13 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 3 5 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 3 5 7 9 11 13 12 11 10 9 8 7 6 5 4 3 5 7 9 11 10 9 8 7 6 5 7 9 11 13 12 11 13 15 14 13 12 11 10 9 8 7 9 11 13 15 17 16 15 14 13 12 11 10 9 8 7 6 5 4 5 7 6 7 9 8 7 9 11 13 15 14 13 12 11 13 15 17 16 15 14 13 12 11 10 9 8 9 11 10 12 1 3 5 7 9 11 13 15 17"));
    Terms terms{o};

    double step = 0.01;
    double best_result = std::numeric_limits<double>::infinity();
    bool prev_satisfied = false;
    size_t MAX_STEPS = 10000000;
    double STEP_MUL = 1.618;
    for (size_t i = 0; i < MAX_STEPS; ++i)
    {
        double val = terms.value();
        double grad = terms.calc_grad();
        if (i % 10000 == 0) {
            bool satisfied = terms.satisfied();
            std::cout << i << "] V " << val << " | GRAD " << grad << " | STEP " << step << " | RES "
                      << terms.satisfied() << std::endl;

            if (val < best_result) {
                best_result = val;
                if (satisfied && prev_satisfied)
                    break;

                prev_satisfied = satisfied;
            }
            else
            {
                std::cout << "FAIL" << std::endl;
            }
        }

        {
            terms.params1 = terms.params;
            terms.params2 = terms.params;

            terms.step(step * STEP_MUL);
            double v2 = terms.value();
            std::swap(terms.params1, terms.params);

            terms.step(step / STEP_MUL);
            double v3 = terms.value();
            std::swap(terms.params2, terms.params);

            if (v2 < val && v2 < v3)
            {
                step *= STEP_MUL;
                terms.params = terms.params1;
            }
            else if (v3 < val && v3 < v2)
            {
                step /= STEP_MUL;
                terms.params = terms.params2;
            }
            else
            {
                step *= 0.5;
            }
        }
    }

    if (!terms.satisfied())
        std::cerr << "FAILED" << std::endl;

    terms.print();

    return 0;
}
