#pragma once

#include <vector>

static inline bool has_parallels_raw(const std::vector<int>& parallels) {
    for (int p : parallels) {
        if (p >= 0) return true;
    }
    return false;
}

static inline bool projective_rotate_raw(
    const std::vector<std::vector<size_t>>& intersections,
    const std::vector<int>& parallels,
    size_t n,
    size_t val,
    std::vector<std::vector<size_t>>& out_intersections,
    std::vector<int>& out_parallels
) {
    if (n == 0) return false;
    if (n % 2 == 0) return false;
    if (val >= n) return false;
    if (intersections.size() != n || parallels.size() != n) return false;
    if (has_parallels_raw(parallels)) return false;
    if (intersections[val].size() != n - 1) return false;
    for (size_t i = 0; i < n; ++i) {
        if (intersections[i].size() != n - 1) return false;
    }

    const size_t ext = n;
    std::vector<int> reord(n + 1, -1);
    for (size_t i = 0; i < n - 1; ++i) {
        size_t line = intersections[val][i];
        if (line >= n) return false;
        if (line == val) return false;
        if (reord[line] >= 0) return false;
        reord[line] = (int)i;
    }
    reord[ext] = (int)(n - 1);

    for (size_t i = 0; i < n; ++i) {
        if (i != val && reord[i] < 0) return false;
    }

    std::vector<std::vector<size_t>> temp(n + 1);
    for (size_t i = 0; i < n; ++i) {
        temp[i] = intersections[i];
        temp[i].push_back(ext);
    }
    temp[ext].resize(n);
    for (size_t i = 0; i < n; ++i) temp[ext][i] = i;

    out_intersections.assign(n, {});
    out_parallels.assign(n, -1);

    for (size_t i = 0; i < n; ++i) {
        size_t v = temp[val][i];
        if (v >= temp.size()) return false;
        const auto& temp_line = temp[v];
        if (temp_line.size() != n) return false;

        size_t val_pos = temp_line.size();
        for (size_t j = 0; j < temp_line.size(); ++j) {
            if (temp_line[j] == val) {
                val_pos = j;
                break;
            }
        }
        if (val_pos == temp_line.size()) return false;

        auto& new_line = out_intersections[i];
        new_line.reserve(n - 1);

        if (v < val) {
            for (size_t j = val_pos; j > 0; --j) {
                size_t old = temp_line[j - 1];
                if (old >= reord.size()) return false;
                int mapped = reord[old];
                if (mapped < 0) return false;
                new_line.push_back((size_t)mapped);
            }
            for (size_t j = n; j > val_pos + 1; --j) {
                size_t old = temp_line[j - 1];
                if (old >= reord.size()) return false;
                int mapped = reord[old];
                if (mapped < 0) return false;
                new_line.push_back((size_t)mapped);
            }
        } else {
            for (size_t j = val_pos + 1; j < n; ++j) {
                size_t old = temp_line[j];
                if (old >= reord.size()) return false;
                int mapped = reord[old];
                if (mapped < 0) return false;
                new_line.push_back((size_t)mapped);
            }
            for (size_t j = 0; j < val_pos; ++j) {
                size_t old = temp_line[j];
                if (old >= reord.size()) return false;
                int mapped = reord[old];
                if (mapped < 0) return false;
                new_line.push_back((size_t)mapped);
            }
        }

        if (new_line.size() != n - 1) return false;
    }

    return true;
}
