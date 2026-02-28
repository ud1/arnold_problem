#ifndef ARN_UTIL_OMATRIX_HPP
#define ARN_UTIL_OMATRIX_HPP

#include <map>
#include <cmath>
#include <unordered_set>
#include <vector>
#include <optional>
#include <cstdint>
#include <system_error>
#include <memory>
#include "bits.hpp"

struct OMatrix;
typedef std::shared_ptr<const OMatrix> OMatrixPtr;
typedef uint16_t Line;

struct OMatrix : std::enable_shared_from_this<OMatrix> {
    OMatrix(const std::vector<std::vector<Line>> &intersections, const std::map<Line, Line> &parallels);

    Line n() const {
        return intersections.size();
    }

    Line k() const {
        return parallels.size() / 2;
    }

    Line get_parallels_ind(Line l) const;
    Line get_val(Line rotation, Line i, Line j) const;
    bool can_rotate(Line rotation) const;
    Line get_line_len(Line rotation, Line i) const;
    Line rotate_line_num(Line rotation, Line l) const;
    OMatrixPtr rotate(Line rotation) const;
    size_t get_num_vertices() const;
    std::vector<Line> get_generators() const;
    OMatrixPtr mirror() const;
    uint64_t hash() const;
    OMatrixPtr min_o() const;
    bool operator==(const OMatrix& other) const {
        return intersections == other.intersections;
    }

    const std::vector<std::vector<Line>> &get_intersections() const {
        return intersections;
    }

    [[nodiscard]] std::string get_eid() const;
private:
    std::vector<std::vector<Line>> intersections;
    std::map<Line, Line> parallels;
    mutable std::optional<uint64_t> cached_hash;
    mutable OMatrixPtr cached_min_o;
    mutable bool is_min_o;
};

namespace std {
    template<>
    struct hash<OMatrix> {
        std::size_t operator()(const OMatrix& o) const noexcept {
            return o.hash();
        }
    };
}

struct OMatrixPtrHash {
    std::size_t operator()(const OMatrixPtr& p) const noexcept {
        return p->hash();
    }
};

struct OMatrixPtrEqual {
    bool operator()(const OMatrixPtr& a, const OMatrixPtr& b) const {
        if (!a && !b)
            return true;
        if (!a || !b)
            return false;
        return *a == *b;
    }
};

std::string base36_encode(uint64_t value);

OMatrixPtr get_OMatrix(std::vector<Line> gens);

void print_OMatrix(const OMatrixPtr &o);

void append_vertical_lines(std::ostream& os, Line count);

void print_wire(std::vector<Line> gens);

void print_wire_condensed(std::vector<Line> gens);

#endif //ARN_UTIL_OMATRIX_HPP
