#pragma once

#include <vector>
#include "common/omatrix.h"

struct EdgeSpec {
    size_t b1, b2, m1, m2, b3, b4, m3, m4;
};

static inline std::vector<EdgeSpec> build_edge_specs(const OMatrix& o) {
    std::vector<EdgeSpec> specs;
    for (size_t i = 0; i < o.n; ++i) {
        const auto& line = o.intersections[i];
        if (line.size() < 2) continue;

        for (size_t idx = 0; idx + 1 < line.size(); ++idx) {
            size_t j = line[idx];
            size_t k = line[idx + 1];
            if (j >= o.n || k >= o.n) continue;
            if (j == i || k == i) continue;

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
