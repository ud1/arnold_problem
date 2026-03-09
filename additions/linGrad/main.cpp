#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <charconv>
#include <cstring>

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
const double MIN_ANGLE = 0.01;

struct EdgeTerm {
    size_t mi, bi, mj, bj, mk, bk;
    double sign;

    EdgeTerm(size_t mi, size_t bi, size_t mj, size_t bj, size_t mk, size_t bk, double sign) : mi(mi), bi(bi), mj(mj),
                                                                                              bj(bj), mk(mk), bk(bk),
                                                                                              sign(sign) {}

    [[nodiscard]] double pure_value2(const std::vector<double> &params) const {
        double result = raw(params);
        if (result < 0.0)
            return 0.0;
        return result * result;
    }

    [[nodiscard]] double value2(const std::vector<double> &params) const
    {
        double result = value1(params);
        return result * result;
    }

    [[nodiscard]] double value1(const std::vector<double> &params) const
    {
        double result = raw(params) + MIN_EDGE;

        if (result < 0.0)
            return 0.0;

        return result;
    }

    [[nodiscard]] double raw(const std::vector<double> &params) const
    {
        // ((bk - bi) * (mi - mj) - (bj - bi) * (mi - mk)) * sign < 0
        return ((params[bk] - params[bi]) * (params[mi] - params[mj])
                        - (params[bj] - params[bi]) * (params[mi] - params[mk])) * sign;
    }

    void grad(const std::vector<double> &params, std::vector<double> &grad_out) const
    {
        double val = value1(params) * 2.0;
        if (val == 0.0)
            return;

        double b1 = (params[bk] - params[bi]) * sign;
        double m1 = (params[mi] - params[mj]) * sign;
        double b2 = (params[bj] - params[bi]) * sign;
        double m2 = (params[mi] - params[mk]) * sign;

        grad_out[mi] += val * (b1 - b2);
        grad_out[bi] += val * (-m1 + m2);
        grad_out[mj] -= val * b1;
        grad_out[bj] -= val * m2;
        grad_out[mk] += val * b2;
        grad_out[bk] += val * m1;
    }
};

struct AngleTerm {
    size_t p1, p2;
    AngleTerm(size_t p1, size_t p2) : p1(p1), p2(p2) {}

    [[nodiscard]] double pure_value2(const std::vector<double> &params, double angle_func_scale) const {
        double result = raw(params);
        if (result < 0.0)
            return 0.0;
        return result * result * angle_func_scale;
    }

    [[nodiscard]] double value2(const std::vector<double> &params, double angle_func_scale) const {
        double result = value1(params);
        return result * result * angle_func_scale;
    }

    [[nodiscard]] double value1(const std::vector<double> &params) const {
        double result = raw(params) + MIN_ANGLE;
        if (result < 0.0)
            return 0.0;

        return result;
    }

    [[nodiscard]] double raw(const std::vector<double> &params) const {
        // m[i] - m[i + 1] < 0
        return params[p1] - params[p2];
    }

    void grad(const std::vector<double> &params, std::vector<double> &grad_out, double angle_func_scale) const {
        double val = value1(params) * 2.0;
        if (val == 0.0)
            return;

        grad_out[p1] += val* angle_func_scale;
        grad_out[p2] -= val* angle_func_scale;
    }
};

struct TermsConfig {
    size_t max_steps = 1000000;
    size_t min_steps = 100000;
    double min_decay = 0.001;
};

struct Terms {
    std::vector<EdgeTerm> edgeTerms;
    std::vector<AngleTerm> angleTerms;
    std::vector<double> params;
    std::vector<double> saved_params;
    std::vector<double> grad;
    std::vector<double> angle_grad;
    std::vector<double> prev_grad;
    double grad_squared;
    size_t edge_not_satisfied;
    size_t edge_contributed;
    size_t angle_not_satisfied;
    size_t angle_contributed;
    double angle_func_scale = 1000.0;
    size_t processed_steps = 0;

    explicit Terms(OMatrix &o) {
        size_t n = o.n;

        for (size_t i = 0; i < o.n; ++i) {
            params.push_back((double) i - (double) o.n * 0.5);
        }
        for (size_t i = 0; i < o.n; ++i) {
            params.push_back(0.0);
        }

        grad_squared = 0.0;
        edge_not_satisfied = 0;
        edge_contributed = 0;
        angle_not_satisfied = 0;
        angle_contributed = 0;

        grad.resize(params.size());
        angle_grad.resize(params.size());
        saved_params.resize(params.size());
        prev_grad.resize(params.size());

        auto m = [](size_t i){return i;}; // index of m[i] param
        auto b = [n](size_t i){return i + n;}; // index of b[i] param

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

                double sign = (i > j) == (i > k) ? +1.0 : -1.0;

                // (bk - bi) * (mi - mj) * sign < (bj - bi) * (mi - mk) * sign
                edgeTerms.emplace_back(m(i), b(i), m(j), b(j), m(k), b(k), sign);
            }
        }
    }

    [[nodiscard]] double value() const
    {
        double result = 0.0;
        for (auto &t : angleTerms)
        {
            result += t.value2(params, angle_func_scale);
        }
        for (auto &t : edgeTerms)
        {
            result += t.value2(params);
        }
        return result;
    }

    [[nodiscard]] double pure_value() const
    {
        double result = 0.0;
        for (auto &t : angleTerms)
        {
            result += t.pure_value2(params, 1000.0);
        }
        for (auto &t : edgeTerms)
        {
            result += t.pure_value2(params);
        }
        return result;
    }

    void calc_grad()
    {
        for (auto &v : grad)
            v = 0.0;

        for (auto &t : angleTerms)
        {
            t.grad(params, grad, angle_func_scale);
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

        grad_squared = result;
    }

    // Обновление angle_func_scale, коэффициент определяет соотношение между вкладами в функцию для angle_grad и edgeTerms.
    // Пытаемся подобрать такое значение, чтобы вклад в градиент уголовых параметров был одинаковый.
    void update_grad_ratio() {
        for (auto &v : grad)
            v = 0.0;

        for (auto &v : angle_grad)
            v = 0.0;

        for (auto &t : angleTerms)
        {
            t.grad(params, angle_grad, angle_func_scale);
        }

        for (auto &t : edgeTerms)
        {
            t.grad(params, grad);
        }

        double grad_edge_angle_squared = 0.0;
        double grad_angle_squared = 0.0;
        for (size_t i = 0; i < params.size() / 2; ++i) {
            grad_angle_squared += angle_grad[i] * angle_grad[i];
            grad_edge_angle_squared += grad[i] * grad[i];
            grad[i] += angle_grad[i];
        }

        if (grad_edge_angle_squared > 0.0 && grad_angle_squared > 0.0)
        {
            double new_ratio_mult = grad_edge_angle_squared / grad_angle_squared;
            new_ratio_mult = (1.0 + new_ratio_mult) * 0.5;
            angle_func_scale *= new_ratio_mult;
            if (angle_func_scale > 100000.0)
                angle_func_scale = 100000.0;
            else if (angle_func_scale < 0.001)
                angle_func_scale = 0.001;
        }
    }

    void step(double v)
    {
        size_t size = params.size();
        for (size_t i = 0; i < size; ++i) {
            params[i] -= grad[i] * v;
        }
    }

    bool satisfied() {
        edge_not_satisfied = 0;
        edge_contributed = 0;
        for (auto &t : edgeTerms)
        {
            double raw = t.raw(params);
            if (raw >= 0.0)
            {
                edge_not_satisfied++;
                edge_contributed++;
            }
            else if (raw + MIN_EDGE > 0)
            {
                edge_contributed++;
            }
        }

        angle_not_satisfied = 0;
        angle_contributed = 0;

        for (auto &t: angleTerms) {
            double raw = t.raw(params);
            if (raw >= 0.0)
            {
                angle_not_satisfied++;
                angle_contributed++;
            }
            else if (raw + MIN_ANGLE > 0)
            {
                angle_contributed++;
            }
        }

        return edge_not_satisfied == 0 && angle_not_satisfied == 0;
    }

    void print() {
        size_t n = params.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            double m = params[i];
            double b = params[i + n];
            std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << "y = " << m << " * x ";
            if (b >= 0.0)
                std::cout << " +";
            std::cout << b << std::endl;
        }
    }

    void print_kv() {
        size_t n = params.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            double m = params[i];
            double b = params[i + n];
            std::cout << "m[" << i << "]:" << std::setprecision(std::numeric_limits<double>::max_digits10) << m << std::endl;
            std::cout << "b[" << i << "]:" << std::setprecision(std::numeric_limits<double>::max_digits10) << b << std::endl;
        }
    }
};

std::vector<std::vector<size_t>> read_file(const std::string &filename) {
    std::vector<std::vector<size_t>> result;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return result;
    }

    std::string line;
    // Read each line from the file and push it into the vector
    while (std::getline(file, line)) {
        auto gens = gens_str_to_vec(line);
        if (!gens.empty())
            result.push_back(gens);
    }

    return result;
}

Terms stretch(const std::vector<size_t> &gens, const TermsConfig &terms_config) {
    OMatrix o = makeOMatrix(gens);
    Terms terms{o};

    double step = 0.01;
    size_t MAX_STEPS = terms_config.max_steps;
    size_t MIN_STEPS = terms_config.min_steps < 60000 ? 10000 : terms_config.min_steps - 50000;
    terms.calc_grad();

    std::vector<double> values;

    terms.processed_steps = 0;
    size_t i;
    for (i = 0; i < MAX_STEPS; ++i)
    {
        terms.prev_grad = terms.grad;
        double val = terms.value();
        terms.calc_grad();

        if (terms.grad_squared == 0.0) {
            break;
        }

//        if (i % 10000 == 0) {
//            bool satisfied = terms.satisfied();
////            std::cout << i << "] V " << val << " | GRAD " << sqrt(terms.grad_squared) << " | STEP " << step << " | RES "
////                      << satisfied << " | ENS " << terms.edge_not_satisfied << " | EC " << terms.edge_contributed
////                      << " | ANS " << terms.angle_not_satisfied << " | AC " << terms.angle_contributed<< std::endl;
//
//            std::cout << i << std::setprecision(10) << ";" << val << ";" << terms.pure_value() << std::endl;
//
//            if (satisfied && prev_satisfied)
//                break;
//
//            prev_satisfied = satisfied;
//        }

        if (i % 10000 == 0 && i > MIN_STEPS) {
            values.push_back(terms.pure_value());
            if (values.size() >= 5) {
                int start = (int) values.size() - 5;
                double max_v = *std::max_element(values.begin() + start, values.end());
                double min_v = *std::min_element(values.begin() + start, values.end());
                double dv = (max_v - min_v) / (max_v + 0.00001);

                if (dv < terms_config.min_decay) {
//                    std::cout << "DV " << i << " " << min_v << " " << max_v << " " << dv << std::endl;
                    break;
                }
            }
        }

        if (terms.satisfied()) {
            break;
        }

        if (i % 100 == 1) {
            terms.update_grad_ratio();
            terms.calc_grad();
            val = terms.value();
            terms.prev_grad = terms.grad;
        }

        for (int j = 0; j < 100; ++j)
        {
            terms.saved_params = terms.params;
            terms.step(step);
            double v1 = terms.value();

            if (v1 - val > -0.5*step*terms.grad_squared) // Armijo–Goldstein Backtracking
            {
                step *= 0.5;
                if (step < 1e-20) {
//                    std::cout << "FAIL" << std::endl;
                    terms.processed_steps = i;
                    return terms;
                }
                terms.params = terms.saved_params; // restore params
            }
            else // Barzilai–Borwein method
            {
                double dgrad_dot_dx = 0;
                for (size_t k = 0; k < terms.grad.size(); ++k) {
                    // dx = step * grad
                    // dgrad = grad - prev_grad
                    dgrad_dot_dx += (terms.grad[k] - terms.prev_grad[k]) * terms.grad[k];
                }
                if (dgrad_dot_dx > 0.0) {
                    double m = terms.grad_squared / dgrad_dot_dx;
                    step = step * m;
                }
                else {
                    step *= 1.1;
                }
                break;
            }
        }
    }

    terms.processed_steps = i;
    return terms;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "Example usage:" << std::endl;
        std::cout << "linGrad --gens \"1 2 3 ...\"" << std::endl;
        std::cout << "linGrad --file file_name --min_steps 100000 --max_steps 1000000 --decay 0.001" << std::endl;
        std::cout << "--decay means a minimum relative change of the pure potential per 50k steps required to continue linearization" << std::endl;
        return -1;
    }
    std::string file_name;
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
        else if (std::string(argv[i]) == "--file") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--file)" << std::endl;
                return -1;
            }
            file_name = argv[i + 1];
        }
        else if (std::string(argv[i]) == "--min_steps") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--min_steps)" << std::endl;
                return -1;
            }
            terms_config.min_steps = std::stoul(argv[i + 1]);
        }
        else if (std::string(argv[i]) == "--max_steps") {
            if (i + 1 == argc)
            {
                std::cerr << "Invalid params (--max_steps)" << std::endl;
                return -1;
            }
            terms_config.max_steps = std::stoul(argv[i + 1]);
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
    }

    std::vector<std::vector<size_t>> confs;
    if (!file_name.empty())
        confs = read_file(file_name);
    else
        confs.push_back(gens_str_to_vec(gens));

    int i = 1;
    for (auto &conf : confs)
    {
        Terms terms = stretch(conf, terms_config);

        if (confs.size() > 1) {
            std::cout << i << " RES " << terms.satisfied() << " " << terms.value() << " " << terms.processed_steps
                      << std::endl;
        }
        else {
            bool satisfied = terms.satisfied();
            std::cout << "RES:" << satisfied << std::endl;
            std::cout << "PVAL:" << terms.pure_value() << std::endl;
            std::cout << "STEPS:" << terms.processed_steps << std::endl;
            std::cout << "PRMS:" << terms.params.size() << std::endl;

            if (satisfied) {
                terms.print_kv();
            }
            else {
                std::cout << "EDGE_NS:" << terms.edge_not_satisfied << std::endl;
                std::cout << "ANGLE_NS:" << terms.angle_not_satisfied << std::endl;
            }
        }

        ++i;
    }
}
