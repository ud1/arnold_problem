#include "omatrix.hpp"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

Line OMatrix::get_val(Line rotation, Line i, Line j) const {
    Line v;
    Line n2 = 2 * n();
    Line l = (i + rotation) % n2;
    if (l < n()) {
        v = intersections[l][j];
    }
    else {
        l = l % n();
        l = get_parallels_ind(l);
        auto &line = intersections[l];
        v = line[line.size() - 1 - j];
    }

    if (rotation < n() && v >= rotation || rotation >= n() && v < rotation % n()) {
        return (v + n2 - rotation) % n();
    }

    v = get_parallels_ind(v);
    return (v + n2 - rotation) % n();
}

OMatrixPtr OMatrix::rotate(Line rotation) const {
    rotation %= (2*n());
    if (rotation == 0)
        return shared_from_this();

    if (!can_rotate(rotation))
        return {};

    std::vector<std::vector<Line>> new_intersections;
    for (int i = 0; i < n(); ++i)
    {
        std::vector<Line> line;
        Line len = get_line_len(rotation, i);
        for (Line j = 0; j < len; ++j)
        {
            line.push_back(get_val(rotation, i, j));
        }
        new_intersections.push_back(line);
    }

    std::map<Line, Line> new_parallels;
    for (auto &p : parallels) {
        new_parallels[rotate_line_num(rotation, p.first)] = rotate_line_num(rotation, p.second);
    }

    return std::make_shared<const OMatrix>(new_intersections, new_parallels);
}

std::vector<Line> OMatrix::get_generators() const {
    std::vector<Line> lines;
    std::vector<Line> pos;
    for (int i = 0; i < n(); ++i)
    {
        lines.push_back(i);
        pos.push_back(0);
    }

    std::vector<Line> generators;

    size_t num_vertices = get_num_vertices();
    for (int i = 0; i < num_vertices; ++i)
    {
        bool found = false;
        for (Line gen = 0; gen < n() - 1; ++gen)
        {
            Line left = lines[gen];
            Line right = lines[gen + 1];
            if (pos[left] < intersections[left].size() && pos[right] < intersections[right].size()
                && intersections[left][pos[left]] == right && intersections[right][pos[right]] == left)
            {
                pos[left]++;
                pos[right]++;
                lines[gen] = right;
                lines[gen + 1] = left;
                generators.push_back(gen);
                found = true;
                break;
            }
        }

        if (!found)
            throw std::runtime_error("Invalid OMatrix");
    }

    return generators;
}

OMatrixPtr OMatrix::mirror() const {
    std::vector<std::vector<Line>> new_intersections;
    for (Line i = n(); i --> 0;)
    {
        std::vector<Line> line;
        for (auto &l : intersections[i])
        {
            line.push_back(n() - 1 - l);
        }

        new_intersections.push_back(line);
    }

    std::map<Line, Line> new_parallels;
    for (auto &p : parallels) {
        new_parallels[n() - 1 - p.first] = n() - 1 - p.second;
    }

    return std::make_shared<const OMatrix>(new_intersections, new_parallels);
}

OMatrixPtr OMatrix::min_o() const {
    if (cached_min_o)
        return cached_min_o;

    if (is_min_o) {
        return shared_from_this();
    }

    uint64_t best_hash = hash();
    OMatrixPtr result = shared_from_this();

    for (Line i = 1; i < 2 * n(); ++i)
    {
        auto r = rotate(i);
        if (r) {
            auto hash = r->hash();
            if (best_hash > hash) {
                best_hash = hash;
                result = r;
            }
        }
    }

    OMatrixPtr mirrored = mirror();
    for (Line i = 0; i < 2 * n(); ++i)
    {
        auto r = mirrored->rotate(i);
        if (r) {
            auto hash = r->hash();
            if (best_hash > hash) {
                best_hash = hash;
                result = r;
            }
        }
    }

    result->is_min_o = true;
    if (!result->is_min_o) // is not self
        cached_min_o = result;
    return result;
}

uint64_t OMatrix::hash() const {
    if (cached_hash.has_value())
        return *cached_hash;

    uint64_t h = 0xcbf29ce484222325ULL;
    const uint64_t p = 0x100000001b3ULL;

    for (auto &line : intersections) {
        for (auto v : line) {
            h ^= (uint64_t) v;
            h *= p;
        }
    }

    cached_hash = h;
    return h;
}

size_t OMatrix::get_num_vertices() const {
    size_t result = 0;
    for (auto &l : intersections)
        result += l.size();

    return result / 2;
}

bool OMatrix::can_rotate(Line rotation) const {
    if (rotation >= 2*n())
        return false;

    Line rotated = rotation % n();
    auto p = parallels.find(rotated);
    if (p == parallels.end())
        return true;

    return p->second >= rotated;
}

Line OMatrix::get_line_len(Line rotation, Line i) const {
    Line l = i + rotation;
    if (l < n()) {
        return intersections[l].size();
    }
    else {
        l = l % n();
        l = get_parallels_ind(l);
        return intersections[l].size();
    }
}

Line OMatrix::rotate_line_num(Line rotation, Line l) const {
    if (l >= rotation)
        return l - rotation;

    return (l + n()*2 - rotation) % n();
}

Line OMatrix::get_parallels_ind(Line l) const {
    auto it = parallels.find(l);
    if (it == parallels.end())
        return l;

    return it->second;
}

OMatrix::OMatrix(const std::vector<std::vector<Line>> &intersections, const std::map<Line, Line> &parallels)
        : intersections(intersections), parallels(parallels) {}

std::string OMatrix::get_eid() const {
    std::string result;

    OMatrixPtr mo = min_o();
    if (mo) {
        result += "E";
        result += std::to_string(mo->n());
        result += "_";
        result += base36_encode(mo->hash());
    }

    return result;
}

const std::string B36_DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

std::string base36_encode(uint64_t value) {
    if (value == 0)
        return "0";

    std::string result;
    while (value > 0) {
        result += B36_DIGITS[value % 36];
        value /= 36;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

OMatrixPtr get_OMatrix(std::vector<Line> gens) {
    Line n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<Line> lines(n);
    std::iota(lines.begin(), lines.end(), 0); // fill 0 1 2 ...
    std::vector<std::vector<Line>> intersections(n);

    for (Line gen : gens)
    {
        Line first = lines[gen];
        Line second = lines[gen + 1];

        intersections[first].push_back(second);
        intersections[second].push_back(first);

        lines[gen] = second;
        lines[gen + 1] = first;
    }

    std::map<Line, Line> parallels;

    for (int i = 0; i < n - 1; ++i)
    {
        if (lines[i] < lines[i + 1])
        {
            if (parallels.count(lines[i]) || parallels.count(lines[i + 1]))
                return {};

            parallels[lines[i]] = lines[i + 1];
            parallels[lines[i + 1]] = lines[i];
        }
    }

    return std::make_shared<const OMatrix>(intersections, parallels);
}

void print_OMatrix(const OMatrixPtr &ptr) {
    if (!ptr)
        return;

    const OMatrix &o = *ptr;

    std::cout << std::endl;

    int max_num_len = std::ceil(std::log10(o.n()));
    for (Line i = 0; i < o.n(); ++i)
    {
        std::cout << std::setw(max_num_len) << i << " || " << std::setw(max_num_len);
        std::cout << o.get_parallels_ind(i);

        std::cout << ") ";

        auto &intersections = o.get_intersections()[i];
        for (Line j = 0; j < intersections.size(); ++j) {
            std::cout << std::setw(max_num_len) << intersections[j];
            if (j + 1 < intersections.size())
                std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void append_vertical_lines(std::ostream &os, Line count) {
    for (Line i = 0; i < count; ++i) {
        if (i > 0) os << "  ";
        os << '|';
    }
}

void print_wire(std::vector<Line> gens) {
    Line n = 0;
    if (!gens.empty()) {
        n = *std::max_element(gens.begin(), gens.end()) + 2;
    }

    append_vertical_lines(std::cout, n);
    std::cout << '\n';

    for (Line g : gens) {
        append_vertical_lines(std::cout, g);
        if (g > 0) {
            std::cout << "  ";
        }
        std::cout << " >< ";
        if (n - g - 2 > 0)
            std::cout << "  ";

        append_vertical_lines(std::cout, n - g - 2);
        std::cout << '\n';
    }

    append_vertical_lines(std::cout, n);
}

void print_wire_condensed(std::vector<Line> gens) {
    Line n = 0;
    if (!gens.empty()) {
        n = *std::max_element(gens.begin(), gens.end()) + 2;
    }

    std::vector<std::optional<Bits>> rows;
    rows.reserve(gens.size());

    for (Line g : gens) {
        Bits bs(n > 0 ? (n - 1) : 0);
        bs.set(g);
        rows.emplace_back(std::move(bs));
    }

    for (Line i = 0; i < rows.size(); ++i) {
        if (!rows[i].has_value())
            continue;

        Line last_j = i;
        for (Line j = i; j > 0; ) {
            --j;
            if (!rows[j].has_value())
                continue;

            if (!is_compatible(*rows[i], *rows[j]))
                break;
            last_j = j;
        }

        if (last_j != i) {
            rows[last_j]->or_with(*rows[i]);
            rows[i].reset(); // null
        }
    }

    append_vertical_lines(std::cout, n);
    std::cout << '\n';

    std::vector<Line> lines(n);
    std::iota(lines.begin(), lines.end(), 0);

    for (auto& opt : rows) {
        if (!opt.has_value()) continue;
        const Bits& bs = *opt;

        for (Line i = 0; i < n * 2 - 1; ++i) {
            if (i % 2 == 0) {
                Line l = i / 2;
                bool cross = ((l > 0) && bs.get(l - 1)) || bs.get(l);
                std::cout << (cross ? ' ' : '|');
            } else {
                Line l = i / 2;
                if (bs.get(l)) {
                    std::cout << "><";
                    std::swap(lines[l], lines[l + 1]);
                } else {
                    std::cout << "  ";
                }
            }
        }
        std::cout << '\n';
    }

    append_vertical_lines(std::cout, n);
}
