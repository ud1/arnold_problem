#include "common/omatrix.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "common/projective_rotation.h"

int OMatrix::norm(int value) const {
    const int nn = (int)n;
    return (value + 2 * nn) % nn;
}

int OMatrix::norm2(int value) const {
    const int nn = (int)n;
    return (value + 4 * nn) % (2 * nn);
}

bool OMatrix::can_rotate(int rotation) const {
    if (rotation < 0 || rotation >= 2 * (int)n) {
        return false;
    }
    int rotated = norm(rotation);
    int p = parallels[(size_t)rotated] >= 0 ? parallels[(size_t)rotated] : rotated;
    return p >= rotated;
}

int OMatrix::rotate_line_num(int rotation, int line) const {
    if (line >= rotation) return line - rotation;
    return norm(line - rotation);
}

int OMatrix::get_line_len(int rotation, int i) const {
    int line = i + rotation;
    if (line < (int)n) {
        return (int)intersections[(size_t)line].size();
    }
    line = norm(line);
    if (parallels[(size_t)line] >= 0) {
        line = parallels[(size_t)line];
    }
    return (int)intersections[(size_t)line].size();
}

int OMatrix::get_val(int rotation, int i, int j) const {
    int line = norm2(i + rotation);
    int v = -1;
    if (line < (int)n) {
        v = (int)intersections[(size_t)line][(size_t)j];
    } else {
        line = norm(line);
        if (parallels[(size_t)line] >= 0) {
            line = parallels[(size_t)line];
        }
        const auto& row = intersections[(size_t)line];
        v = (int)row[row.size() - 1 - (size_t)j];
    }

    if ((rotation < (int)n && v >= rotation) ||
        (rotation >= (int)n && v < norm(rotation))) {
        return norm(v - rotation);
    }

    if (parallels[(size_t)v] >= 0) {
        v = parallels[(size_t)v];
    }
    return norm(v - rotation);
}

bool OMatrix::rotate(int rotation, OMatrix& out) const {
    if (n == 0) return false;
    int rot = rotation % (2 * (int)n);
    if (rot < 0) rot += 2 * (int)n;
    if (rot == 0) {
        out = *this;
        return true;
    }
    if (!can_rotate(rot)) return false;

    OMatrix r;
    r.n = n;
    r.num_vertices = num_vertices;
    r.intersections.assign(n, {});
    r.parallels.assign(n, -1);

    for (size_t i = 0; i < n; ++i) {
        int len = get_line_len(rot, (int)i);
        r.intersections[i].reserve((size_t)len);
        for (int j = 0; j < len; ++j) {
            r.intersections[i].push_back((size_t)get_val(rot, (int)i, j));
        }
    }

    for (size_t k = 0; k < n; ++k) {
        int v = parallels[k];
        if (v < 0) continue;
        int nk = rotate_line_num(rot, (int)k);
        int nv = rotate_line_num(rot, v);
        r.parallels[(size_t)nk] = nv;
    }

    out = std::move(r);
    return true;
}

OMatrix OMatrix::reflect() const {
    OMatrix r;
    r.n = n;
    r.num_vertices = num_vertices;
    r.intersections.assign(n, {});
    r.parallels.assign(n, -1);

    for (size_t old = 0; old < n; ++old) {
        size_t ni = n - 1 - old;
        const auto& row = intersections[old];
        auto& out_row = r.intersections[ni];
        out_row.reserve(row.size());
        for (size_t v : row) out_row.push_back(n - 1 - v);
    }

    for (size_t k = 0; k < n; ++k) {
        if (parallels[k] < 0) continue;
        int nk = (int)(n - 1 - k);
        int nv = (int)(n - 1 - (size_t)parallels[k]);
        r.parallels[(size_t)nk] = nv;
    }

    return r;
}

bool OMatrix::has_parallels() const {
    return has_parallels_raw(parallels);
}

bool OMatrix::projective_rotate(size_t val, OMatrix& out) const {
    OMatrix r;
    r.n = n;
    r.num_vertices = num_vertices;
    if (!projective_rotate_raw(intersections, parallels, n, val, r.intersections, r.parallels)) {
        return false;
    }
    out = std::move(r);
    return true;
}

bool validate_omatrix(const OMatrix& o, bool require_full_rows) {
    if (o.intersections.size() != o.n) return false;
    if (o.parallels.size() != o.n) return false;

    std::vector<std::vector<unsigned char>> seen(o.n, std::vector<unsigned char>(o.n, 0));
    for (size_t i = 0; i < o.n; ++i) {
        int p = o.parallels[i];
        if (p < -1 || p >= (int)o.n) return false;
        if (p >= 0) {
            if ((size_t)p == i) return false;
            if (o.parallels[(size_t)p] != (int)i) return false;
        }

        const auto& row = o.intersections[i];
        if (require_full_rows && row.size() != o.n - 1) return false;
        if (row.size() > o.n - 1) return false;
        for (size_t v : row) {
            if (v >= o.n) return false;
            if (v == i) return false;
            if (seen[i][v]) return false;
            seen[i][v] = 1;
        }
    }

    for (size_t i = 0; i < o.n; ++i) {
        for (size_t j = i + 1; j < o.n; ++j) {
            if (seen[i][j] != seen[j][i]) return false;
        }
    }
    return true;
}

OMatrix make_omatrix(const std::vector<size_t>& gens) {
    if (gens.empty()) {
        throw std::runtime_error("Empty generator list");
    }

    const size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);

    std::vector<std::vector<size_t>> intersections(n);
    for (size_t gen : gens) {
        if (gen + 1 >= n) {
            throw std::runtime_error("Generator index out of range");
        }
        size_t first = lines[gen];
        size_t second = lines[gen + 1];
        intersections[first].push_back(second);
        intersections[second].push_back(first);
        lines[gen] = second;
        lines[gen + 1] = first;
    }

    std::vector<int> parallels(n, -1);
    for (size_t i = 0; i + 1 < n; ++i) {
        if (lines[i] < lines[i + 1]) {
            parallels[lines[i]] = (int)lines[i + 1];
            parallels[lines[i + 1]] = (int)lines[i];
        }
    }

    OMatrix out{intersections, parallels, n, gens.size()};
    if (!validate_omatrix(out, false)) {
        throw std::runtime_error("Invalid O-matrix built from generators");
    }
    return out;
}

std::vector<size_t> omatrix_to_generators(const OMatrix& o) {
    const size_t n = o.n;
    if (n < 2) return {};

    size_t incidences = 0;
    for (const auto& row : o.intersections) incidences += row.size();
    if (incidences % 2 != 0) {
        throw std::runtime_error("Invalid O-matrix: odd incidence count");
    }
    const size_t steps = incidences / 2;

    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);
    std::vector<size_t> pos(n, 0);
    std::vector<size_t> gens(steps, 0);

    for (size_t step = 0; step < steps; ++step) {
        bool found = false;
        for (size_t gen = 0; gen + 1 < n; ++gen) {
            size_t left = lines[gen];
            size_t right = lines[gen + 1];
            if (pos[left] >= o.intersections[left].size()) continue;
            if (pos[right] >= o.intersections[right].size()) continue;
            if (o.intersections[left][pos[left]] != right) continue;
            if (o.intersections[right][pos[right]] != left) continue;

            ++pos[left];
            ++pos[right];
            std::swap(lines[gen], lines[gen + 1]);
            gens[step] = gen;
            found = true;
            break;
        }
        if (!found) {
            throw std::runtime_error("Invalid O-matrix: cannot convert to generators");
        }
    }

    for (size_t line = 0; line < n; ++line) {
        if (pos[line] != o.intersections[line].size()) {
            throw std::runtime_error("Invalid O-matrix: unconsumed intersections");
        }
    }

    return gens;
}
