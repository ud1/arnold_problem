#ifndef ARN_UTIL_CONF_HPP
#define ARN_UTIL_CONF_HPP
#include "omatrix.hpp"

struct Configuration {
    int line_number;
    std::vector<Line> gens;
    OMatrixPtr o;

    Configuration(int line_number, const std::vector<Line> &gens) : line_number(line_number), gens(gens) {
        o = get_OMatrix(gens);
    }

    [[nodiscard]] OMatrixPtr get_min_o() const {
        if (!o)
            return {};
        return o->min_o();
    }
};

#endif //ARN_UTIL_CONF_HPP
