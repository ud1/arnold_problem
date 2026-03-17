#pragma once

#include <cstddef>
#include <vector>

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    std::vector<int> parallels;
    size_t n = 0;
    size_t num_vertices = 0;

    int norm(int value) const;
    int norm2(int value) const;
    bool can_rotate(int rotation) const;
    int rotate_line_num(int rotation, int line) const;
    int get_line_len(int rotation, int i) const;
    int get_val(int rotation, int i, int j) const;
    bool rotate(int rotation, OMatrix& out) const;
    OMatrix reflect() const;
    bool has_parallels() const;
    bool projective_rotate(size_t val, OMatrix& out) const;
};

bool validate_omatrix(const OMatrix& o, bool require_full_rows = false);
OMatrix make_omatrix(const std::vector<size_t>& gens);
std::vector<size_t> omatrix_to_generators(const OMatrix& o);
